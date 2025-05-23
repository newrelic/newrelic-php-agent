/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file handles the initialization that happens at the beginning of
 * each request.
 */
#include "php_agent.h"
#include "php_error.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_user_instrument.h"
#include "nr_datastore_instance.h"
#include "nr_txn.h"
#include "nr_rum.h"
#include "nr_slowsqls.h"
#include "util_logging.h"
#include "util_strings.h"
#include "util_syscalls.h"

static void nr_php_datastore_instance_destroy(
    nr_datastore_instance_t* instance) {
  nr_datastore_instance_destroy(&instance);
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
/* OAPI global stacks (as opposed to call stack used previously) 
 * need to have a dtor set so that when we free it
 * during rshutdown, all elements are properly freed */
static void str_stack_dtor(void* e, NRUNUSED void* d) {
  char* str = (char*)e;
  nr_free(str);
}
static void zval_stack_dtor(void* e, NRUNUSED void* d) {
  zval* zv = (zval*)e;
  nr_php_zval_free(&zv);
}
#endif

#ifdef TAGS
void zm_activate_newrelic(void); /* ctags landing pad only */
#endif
PHP_RINIT_FUNCTION(newrelic) {
  (void)type;
  (void)module_number;

  NRPRG(current_framework) = NR_FW_UNSET;
  NRPRG(framework_version) = 0;
  NRPRG(php_cur_stack_depth) = 0;
  NRPRG(deprecated_capture_request_parameters) = NRINI(capture_params);
  NRPRG(sapi_headers) = NULL;
  NRPRG(error_group_user_callback).is_set = false;
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
#if ZEND_MODULE_API_NO == ZEND_7_4_X_API_NO
  nr_php_init_user_instrumentation();
#endif
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG(drupal_http_request_segment) = NULL;
  NRPRG(drupal_http_request_depth) = 0;
#endif
#else
  NRPRG(pid) = nr_getpid();
  NRPRG(user_function_wrappers) = nr_vector_create(64, NULL, NULL);
#endif

  if ((0 == NR_PHP_PROCESS_GLOBALS(enabled)) || (0 == NRINI(enabled))) {
    return SUCCESS;
  }

  /*
   * Ensure that all late initialisation tasks are complete before starting any
   * transactions.
   */
  nr_php_global_once(nr_php_late_initialization);

  nrl_verbosedebug(NRL_INIT, "RINIT processing started");

  nr_php_exception_filters_init(&NRPRG(exception_filters));
  nr_php_exception_filters_add(&NRPRG(exception_filters),
                               nr_php_ignore_exceptions_ini_filter);

  /*
   * Trigger the _SERVER and _REQUEST auto-globals to initialize.
   *
   * The _SERVER globals can be accessed through PG
   * (http_globals)[TRACK_VARS_SERVER]) See nr_php_get_server_global
   *
   * The _REQUEST globals can be accessed through zend_hash_find (&EG
   * (symbol_table), NR_HSTR ("_REQUEST"), ...
   */
  nr_php_zend_is_auto_global(NR_PSTR("_SERVER") TSRMLS_CC);
  nr_php_zend_is_auto_global(NR_PSTR("_REQUEST") TSRMLS_CC);

  nr_php_capture_sapi_headers(TSRMLS_C);

  /*
   * Add an exception handler so we can better handle uncaught exceptions.
   */
  nr_php_error_install_exception_handler(TSRMLS_C);

  /*
   * Instrument extensions if we've been asked to and it hasn't already
   * happened.
   */
  if ((NR_PHP_PROCESS_GLOBALS(instrument_extensions))
      && (NULL == NRPRG(extensions))) {
    NRPRG(extensions) = nr_php_extension_instrument_create();
    nr_php_extension_instrument_rescan(NRPRG(extensions) TSRMLS_CC);
  }

  NRPRG(check_cufa) = false;

  /*
   * Pre-OAPI, this variables were kept on the call stack and
   * therefore had no need to be in an nr_stack
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG(check_cufa) = false;
  nr_stack_init(&NRPRG(predis_ctxs), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG(wordpress_tags), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG(wordpress_tag_states), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG(drupal_invoke_all_hooks), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG(drupal_invoke_all_states), NR_STACK_DEFAULT_CAPACITY);
  NRPRG(predis_ctxs).dtor = str_stack_dtor;
  NRPRG(drupal_invoke_all_hooks).dtor = zval_stack_dtor;
#endif

  NRPRG(mysql_last_conn) = NULL;
  NRPRG(pgsql_last_conn) = NULL;
  NRPRG(datastore_connections) = nr_hashmap_create(
      (nr_hashmap_dtor_func_t)nr_php_datastore_instance_destroy);

  nr_php_txn_begin(0, 0 TSRMLS_CC);

  nrl_verbosedebug(NRL_INIT, "RINIT processing done");

  return SUCCESS;
}
