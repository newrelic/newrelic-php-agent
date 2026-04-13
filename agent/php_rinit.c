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
#include "php_user_instrument_wraprec_hashmap.h"
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

int nr_php_sapi_activate(void) {
  int result = 0;

  /*
   * Chain to the original sapi_module.activate if one existed.
   */
  if (NR_PHP_PROCESS_GLOBALS(orig_sapi_activate)) {
    result = NR_PHP_PROCESS_GLOBALS(orig_sapi_activate)();
  }

  /*
   * Gate: only fire per-request begin if RINIT already ran (worker mode).
   * In classic mode, sapi_activate fires before RINIT inside
   * php_request_startup(), so rinit_active is false — skip and let the
   * engine call RINIT via zend_activate_modules().
   */
  if (!NRPRG_SHARED(rinit_active)) {
    return result;
  }

  NRPRG_SHARED(worker_request_active) = true;

  /*
   * Worker per-request begin: lightweight transaction cycle instead of full
   * RINIT. Modeled on Laravel Queue Worker::process before callback
   * (fw_laravel_queue.c:319-343).
   *
   * We do NOT call full PHP_RINIT because RINIT:
   *   - resets current_framework (can't be re-detected, OAPI cached)
   *   - reinits wraprec hashmaps (destroys instrumentation)
   *   - runs late_initialization (once per process)
   *   - sets rinit_active (must stay true for worker lifetime)
   *   - installs exception handler (already installed at boot)
   */

  /* Discard the idle transaction from the previous sapi_deactivate
   * (or the boot-time RINIT if this is the first worker request). */
  nr_php_txn_end(1, 0 TSRMLS_CC);

  /* Start the real request transaction. */
  nr_php_txn_begin(0, 0 TSRMLS_CC);

  /* Per-request state init — duplicated from RINIT (php_rinit.c:202-258).
   * These must be re-initialized each request. */

  /* Exception filters: from RINIT:202-204 */
  nr_php_exception_filters_init(&NRPRG_SHARED(exception_filters));
  nr_php_exception_filters_add(&NRPRG_SHARED(exception_filters),
                               nr_php_ignore_exceptions_ini_filter);

  /* Trigger superglobals: from RINIT:215-216 */
  nr_php_zend_is_auto_global(NR_PSTR("_SERVER") TSRMLS_CC);
  nr_php_zend_is_auto_global(NR_PSTR("_REQUEST") TSRMLS_CC);

  /* Capture SAPI headers: from RINIT:218, RINIT:269.
   * NULL the pointer first to prevent stale pointer if header capture fails. */
  NRPRG_SHARED(sapi_headers) = NULL;
  nr_php_capture_sapi_headers(TSRMLS_C);

  /*
   * error_group_user_callback is deliberately NOT reset here. In classic mode,
   * RINIT resets it per-request (RINIT:270). In worker mode, the callback is
   * typically registered during worker bootstrap (outside frankenphp_handle_request)
   * and should persist across requests. Resetting it would silently disable
   * the user's error group callback after the first request.
   */

  /* Reset per-request tracking state: from RINIT:235-256 */
  NRPRG_CTX(php_cur_stack_depth) = 0;
  NRPRG_CTX(check_cufa) = false;

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(drupal_http_request_depth) = 0;
  nr_stack_init(&NRPRG_CTX(predis_ctxs), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(wordpress_tags), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(wordpress_tag_states), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(drupal_invoke_all_hooks), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(drupal_invoke_all_states), NR_STACK_DEFAULT_CAPACITY);
  NRPRG_CTX(predis_ctxs).dtor = str_stack_dtor;
  NRPRG_CTX(drupal_invoke_all_hooks).dtor = zval_stack_dtor;
#endif

  /* Reset per-request DB state: from RINIT:253-256 */
  NRPRG_CTX(mysql_last_conn) = NULL;
  NRPRG_CTX(pgsql_last_conn) = NULL;
  NRPRG_CTX(datastore_connections) = nr_hashmap_create(
      (nr_hashmap_dtor_func_t)nr_php_datastore_instance_destroy);

  return result;
}

int nr_php_sapi_deactivate(void) {
  int result = 0;

  /*
   * Gate: only fire per-request end if our sapi_activate fired per-request
   * begin. This prevents RSHUTDOWN from firing on:
   *   - classic mode (sapi_deactivate after RSHUTDOWN, rinit_active=false)
   *   - dummy request teardown (sapi_deactivate without preceding
   *     worker-mode sapi_activate, worker_request_active=false)
   *   - worker shutdown (after RSHUTDOWN, rinit_active=false)
   */
  if (!NRPRG_SHARED(worker_request_active)) {
    goto chain_original;
  }

  NRPRG_SHARED(worker_request_active) = false;

  /*
   * Worker per-request end: lightweight transaction cycle + per-request
   * cleanup instead of full RSHUTDOWN + post_deactivate. Modeled on
   * Laravel Queue Worker::process after callback (fw_laravel_queue.c:355-396).
   *
   * We do NOT call full PHP_RSHUTDOWN or nr_php_post_deactivate because:
   *   - RSHUTDOWN clears rinit_active (must stay true for worker lifetime)
   *   - RSHUTDOWN calls nr_guzzle4_rshutdown (class uninheritance for executor
   *     shutdown — executor stays alive in worker mode)
   *   - RSHUTDOWN calls nr_curl_rshutdown, nr_php_pdo_rshutdown,
   *     nr_php_mysqli_rshutdown (early zval cleanup for post_deactivate timing
   *     — not needed since txn_end runs with executor alive)
   *   - post_deactivate destroys wraprec hashmaps (can't be rebuilt, OAPI cached)
   *   - post_deactivate destroys WordPress matchers (can't be rebuilt)
   *   - post_deactivate resets current_framework (can't be re-detected)
   *   - post_deactivate calls nr_php_reset_user_instrumentation (no-op on PHP 8+)
   */

  /* End and record the request transaction. With in_post_deactivate=0,
   * txn_end internally calls nr_php_txn_do_shutdown (captures REQUEST_URI
   * + request params), adds metrics, sends to daemon, then frees all
   * transaction-level hashmaps (curl_multi_metadata, pdo_link_options,
   * mysqli_queries, etc). See php_txn.c:1257-1360.
   * This subsumes what RSHUTDOWN + post_deactivate do separately in
   * classic mode. */
  nr_php_txn_end(0, 0 TSRMLS_CC);

  /* No idle txn_begin here. Unlike the Laravel Queue pattern
   * (fw_laravel_queue.c:394), we cannot call nr_php_txn_begin in
   * sapi_deactivate because FrankenPHP's frankenphp_worker_request_shutdown
   * calls php_output_deactivate() BEFORE sapi_deactivate(). txn_begin
   * initializes output buffering which crashes on the deactivated output
   * stack (SIGSEGV in zend_stack_push → memcpy, confirmed in run-6).
   *
   * sapi_activate handles both txn_end(discard) + txn_begin at the start
   * of the next request, where the output system is alive. Between requests,
   * NRPRG(txn) is NULL — the agent is not recording. */

  /* Per-request cleanup — duplicated from post_deactivate (php_rshutdown.c:98-176).
   * Only the per-request subset; persistent state (hashmaps, matchers,
   * framework) is deliberately preserved. */

  /* Remove transient wraprecs: from post_deactivate:121.
   * WordPress/Drupal hooks, Predis connections, exception handlers —
   * all created per-request by app execution, will be recreated next request. */
  nr_php_remove_transient_user_instrumentation();

  /* Destroy exception filters: from post_deactivate:129.
   * Paired with init in sapi_activate. */
  nr_php_exception_filters_destroy(&NRPRG_SHARED(exception_filters));

  /* Free per-request DB state: from post_deactivate:137-139. */
  nr_free(NRPRG_CTX(mysql_last_conn));
  nr_free(NRPRG_CTX(pgsql_last_conn));
  nr_hashmap_destroy(&NRPRG_CTX(datastore_connections));

  /* Destroy per-request stacks: from post_deactivate:146-154. */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_stack_destroy_fields(&NRPRG_CTX(wordpress_tags));
  nr_stack_destroy_fields(&NRPRG_CTX(wordpress_tag_states));
  nr_stack_destroy_fields(&NRPRG_CTX(drupal_invoke_all_hooks));
  nr_stack_destroy_fields(&NRPRG_CTX(drupal_invoke_all_states));
  nr_stack_destroy_fields(&NRPRG_CTX(predis_ctxs));
#endif
  nr_hashmap_destroy(&NRPRG_CTX(predis_commands));

  /* Clear stale per-request pointers: from post_deactivate:166-171.
   * sapi_headers is NULLed here because orig_sapi_deactivate (chained below)
   * cleans up SAPI globals, potentially invalidating the pointer. */
  NRPRG_CTX(cufa_callback) = NULL;
  NRPRG_SHARED(sapi_headers) = NULL;
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(drupal_http_request_segment) = NULL;
#endif

chain_original:
  if (NR_PHP_PROCESS_GLOBALS(orig_sapi_deactivate)) {
    result = NR_PHP_PROCESS_GLOBALS(orig_sapi_deactivate)();
  }

  return result;
}

#ifdef TAGS
void zm_activate_newrelic(void); /* ctags landing pad only */
#endif
PHP_RINIT_FUNCTION(newrelic) {
  (void)type;
  (void)module_number;

  NRPRG_SHARED(rinit_active) = true;

  NRPRG_SHARED(current_framework) = NR_FW_UNSET;
  NRPRG_CTX(php_cur_stack_depth) = 0;
  NRPRG_CTX(deprecated_capture_request_parameters) = NRINI(capture_params);
  NRPRG_SHARED(sapi_headers) = NULL;
  NRPRG_CTX(error_group_user_callback).is_set = false;
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
#if ZEND_MODULE_API_NO == ZEND_7_4_X_API_NO
  nr_php_init_user_instrumentation();
#endif
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(drupal_http_request_segment) = NULL;
  NRPRG_CTX(drupal_http_request_depth) = 0;
#endif
#else
  NRPRG_SHARED(pid) = nr_getpid();
  NRPRG_SHARED(user_function_wrappers) = nr_vector_create(64, NULL, NULL);
#endif

  /* initialization of transient wraprecs which are per request globals */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  NRPRG_SHARED(transient_wraprecs) = NULL;
  nrl_verbosedebug(NRL_INSTRUMENT, "%s: initialized transient_wraprecs to NULL",
                   __func__);

#ifdef ZTS
  /* ZTS: deep copy INI hashmaps into per-request hashmaps. */
  nr_php_user_instrument_wraprec_hashmap_replay_ini();
  nrl_verbosedebug(NRL_INSTRUMENT, "%s: replayed INI wraprec hashmaps",
                   __func__);
#endif
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

  nr_php_exception_filters_init(&NRPRG_SHARED(exception_filters));
  nr_php_exception_filters_add(&NRPRG_SHARED(exception_filters),
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
      && (NULL == NRPRG_SHARED(extensions))) {
    NRPRG_SHARED(extensions) = nr_php_extension_instrument_create();
    nr_php_extension_instrument_rescan(NRPRG_SHARED(extensions) TSRMLS_CC);
  }

  NRPRG_CTX(check_cufa) = false;

  /*
   * Pre-OAPI, this variables were kept on the call stack and
   * therefore had no need to be in an nr_stack
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  NRPRG_CTX(check_cufa) = false;
  nr_stack_init(&NRPRG_CTX(predis_ctxs), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(wordpress_tags), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(wordpress_tag_states), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(drupal_invoke_all_hooks), NR_STACK_DEFAULT_CAPACITY);
  nr_stack_init(&NRPRG_CTX(drupal_invoke_all_states),
                NR_STACK_DEFAULT_CAPACITY);
  NRPRG_CTX(predis_ctxs).dtor = str_stack_dtor;
  NRPRG_CTX(drupal_invoke_all_hooks).dtor = zval_stack_dtor;
#endif

  NRPRG_CTX(mysql_last_conn) = NULL;
  NRPRG_CTX(pgsql_last_conn) = NULL;
  NRPRG_CTX(datastore_connections) = nr_hashmap_create(
      (nr_hashmap_dtor_func_t)nr_php_datastore_instance_destroy);

  nr_php_txn_begin(0, 0 TSRMLS_CC);

  nrl_verbosedebug(NRL_INIT, "RINIT processing done");

  return SUCCESS;
}
