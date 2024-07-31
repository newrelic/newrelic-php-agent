/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_error.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "php_hash.h"
#include "fw_hooks.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Sets the web transaction name. If strip_base == true,
 * leading class path components will be stripped.
 */
static int nr_lumen_name_the_wt(const char* name TSRMLS_DC,
                                const char* lumen_version,
                                bool strip_base) {
  const char* path = NULL;

  if (NULL == name) {
    return NR_FAILURE;
  }

  if (strip_base) {
    path = strrchr(name, '\\');
    // Backslash was not found
    if (NULL == path) {
      path = name;
    } else {
      path += 1;
    }
  } else {
    path = name;
  }

  nr_txn_set_path(
      lumen_version, NRPRG(txn), path, NR_PATH_TYPE_ACTION,
      NR_OK_TO_OVERWRITE); /* Watch out: this name is OK to overwrite */

  return NR_SUCCESS;
}

/*
 * Wrapper around nr_lumen_name_the_wt for zval strings
 */
static int nr_lumen_name_the_wt_from_zval(const zval* name TSRMLS_DC,
                                          const char* lumen_version,
                                          bool strip_base) {
  int rc = NR_FAILURE;
  if (nrlikely(nr_php_is_zval_non_empty_string(name))) {
    char* name_str = nr_strndup(Z_STRVAL_P(name), Z_STRLEN_P(name));
    rc = nr_lumen_name_the_wt(name_str TSRMLS_CC, lumen_version, strip_base);
    nr_free(name_str);
  }

  return rc;
}

/*
 * Core transaction naming logic. Wraps the function that correlates
 * requests to routes
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the last wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_lumen_handle_found_route) {
  zval* route_info = NULL;

  /* Warning avoidance */
  (void)wraprec;

  /* Verify that we are using Lumen, otherwise bail. */
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LUMEN);

  /* $routeInfo object used by Application */
  route_info = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /* We expect route_info to be an array.  At index 1, if we see an
   * 'as' key, then we have access to the route, otherwise, if we have
   * a 'uses' key we have access to the controller and action.
   * See: https://lumen.laravel.com/docs/8.x/routing#named-routes
   */
  if (!nr_php_is_zval_valid_array(route_info)) {
    nrl_verbosedebug(NRL_TXN, "Lumen: $routeInfo was not an array");
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  /* obtain $routeInfo[1] */
  zend_ulong idx = 1;
  zval* route_info_pos
      = nr_php_zend_hash_index_find(Z_ARRVAL_P(route_info), idx);

  /* obtain $routeInfo[1]['as'] for route name */
  zval* route_name = NULL;
  if (NULL != route_info_pos) {
    route_name = nr_php_zend_hash_find(Z_ARRVAL_P(route_info_pos), "as");
  }

  if (NULL != route_name) {
    if (NR_SUCCESS
        != nr_lumen_name_the_wt_from_zval(route_name TSRMLS_CC, "Lumen",
                                          false)) {
      nrl_verbosedebug(NRL_TXN, "Lumen: located route name is a non-string");
    }
  } else {
    /* No route located, use controller instead */
    nrl_verbosedebug(
        NRL_TXN,
        "Lumen: unable locate route, attempting to use controller instead");

    /* obtain $routeInfo[1]['uses'] for controller name */
    zval* controller_name
        = nr_php_zend_hash_find(Z_ARRVAL_P(route_info_pos), "uses");

    if (NULL != controller_name) {
      if (NR_SUCCESS
          != nr_lumen_name_the_wt_from_zval(controller_name TSRMLS_CC, "Lumen",
                                            true)) {
        nrl_verbosedebug(NRL_TXN,
                         "Lumen: located controller name is a non-string");
      }

    } else {
      nrl_verbosedebug(NRL_TXN, "Lumen: unable to locate controller or route");
    }
  }

end:
  nr_php_arg_release(&route_info);
}
NR_PHP_WRAPPER_END

/*
 * Exception handling logic. Wraps the function that routes
 * exceptions to their respective handlers
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This entails that the last wrapped call gets to name the
 * txn which in this case is the one that generated the exception.
 */
NR_PHP_WRAPPER(nr_lumen_exception) {
  zval* exception = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LUMEN);

#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  const char* class_name = NULL;
  const char* ignored = NULL;
#else
  char* class_name = NULL;
  char* ignored = NULL;
#endif /* PHP >= 5.4 */

  char* name = NULL;

  /*
   * When the exception handler renders the response, name the transaction
   * after the exception handler using the same format used for controller
   * actions. e.g. Controller@action.
   */
  class_name = get_active_class_name(&ignored TSRMLS_CC);
  name = nr_formatf("%s@%s", class_name, get_active_function_name(TSRMLS_C));
  nr_lumen_name_the_wt(name TSRMLS_CC, "Lumen", true);
  nr_free(name);

  exception = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == exception) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: $e is NULL", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  nr_status_t st;
  int priority = nr_php_error_get_priority(E_ERROR);

  st = nr_php_error_record_exception(NRPRG(txn), exception, priority,
                                     true /* add to segment */,
                                     NULL /* use default prefix */,
                                     &NRPRG(exception_filters) TSRMLS_CC);

  if (NR_FAILURE == st) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: unable to record exception", __func__);
  }

end:
  nr_php_arg_release(&exception);
}
NR_PHP_WRAPPER_END

void nr_lumen_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent having to name routing errors.
   * This follows what is done in the symfony logic
   */
  nr_txn_set_path("Lumen", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  nr_php_wrap_user_function(
      NR_PSTR("Laravel\\Lumen\\Application::handleFoundRoute"),
      nr_lumen_handle_found_route TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Laravel\\Lumen\\Application::sendExceptionToHandler"),
      nr_lumen_exception, NULL);
#else
  nr_php_wrap_user_function(
      NR_PSTR("Laravel\\Lumen\\Application::sendExceptionToHandler"),
      nr_lumen_exception TSRMLS_CC);
#endif

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "laravel/lumen-framework",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
