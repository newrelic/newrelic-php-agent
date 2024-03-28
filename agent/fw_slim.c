/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nr_txn.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"

static char* nr_slim_path_from_route(zval* route TSRMLS_DC) {
  zval* name = NULL;
  zval* pattern = NULL;

  name = nr_php_get_zval_object_property(route, "name" TSRMLS_CC);
  if (name) {
    if (nr_php_is_zval_non_empty_string(name)) {
      return nr_strndup(Z_STRVAL_P(name), Z_STRLEN_P(name));
    }
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "Slim: unable to read route name property");
  }

  pattern = nr_php_get_zval_object_property(route, "pattern" TSRMLS_CC);
  if (pattern) {
    if (nr_php_is_zval_non_empty_string(pattern)) {
      return nr_strndup(Z_STRVAL_P(pattern), Z_STRLEN_P(pattern));
    }
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Slim: unable to read route pattern property");
  }

  return NULL;
}

/*
 * Wrap the \Slim\Route::dispatch method, which is the happy path for Slim 2.x
 * routing. i.e. The router has succesfully matched the URL and dispatched the
 * request to a route.
 *
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_slim2_route_dispatch) {
  zval* this_var = NULL;
  zval** retval_ptr = NULL;
  char* txn_name = NULL;

  (void)wraprec;
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SLIM);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  txn_name = nr_slim_path_from_route(this_var TSRMLS_CC);
  nr_php_scope_release(&this_var);

  retval_ptr = NR_GET_RETURN_VALUE_PTR;

  NR_PHP_WRAPPER_CALL;

  /*
   * Route::dispatch returns true if it handled the request; otherwise, false.
   */

  if (txn_name && retval_ptr && nr_php_is_zval_true(*retval_ptr)) {
    nr_txn_set_path("Slim", NRPRG(txn), txn_name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }

  nr_free(txn_name);
}
NR_PHP_WRAPPER_END

/*
 * Wrap the \Slim\Route::run method, which is the happy path for Slim routing.
 * i.e. The router has succesfully matched the URL and dispatched the request
 * to a route.
 *
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_slim3_4_route_run) {
  zval* this_var = NULL;
  char* txn_name = NULL;

  (void)wraprec;
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SLIM);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  txn_name = nr_slim_path_from_route(this_var TSRMLS_CC);
  nr_php_scope_release(&this_var);

  NR_PHP_WRAPPER_CALL;

  if (txn_name) {
    nr_txn_set_path("Slim", NRPRG(txn), txn_name, NR_PATH_TYPE_ACTION,
                    NR_NOT_OK_TO_OVERWRITE);
    nr_free(txn_name);
  }
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_slim_application_construct) {
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS);
  char* version = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  version = nr_php_get_object_constant(this_var, "VERSION");
  
  // Add php package to transaction
  nr_txn_add_php_package(NRPRG(txn), "slim/slim", version);

  nr_fw_support_add_package_supportability_metric(NRPRG(txn), "slim/slim",
                                                  version);

  nr_free(version);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_slim_enable(TSRMLS_D) {
  NR_UNUSED_TSRMLS;

  nr_php_wrap_user_function(NR_PSTR("Slim\\Route::dispatch"),
                            nr_slim2_route_dispatch TSRMLS_CC);
  /* Slim 3 */
  nr_php_wrap_user_function(NR_PSTR("Slim\\Route::run"),
                            nr_slim3_4_route_run TSRMLS_CC);
  /* Slim 4 */
  nr_php_wrap_user_function(NR_PSTR("Slim\\Routing\\Route::run"),
                            nr_slim3_4_route_run TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Slim 2 does not have the same path as Slim 3/4 which is why
      we need to separate these*/
    nr_php_wrap_user_function(NR_PSTR("Slim\\Slim::__construct"),
                              nr_slim_application_construct);

    nr_php_wrap_user_function(NR_PSTR("Slim\\App::__construct"),
                              nr_slim_application_construct);
  }
}
