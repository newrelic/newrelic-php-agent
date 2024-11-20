/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"

#define PHP_PACKAGE_NAME "cakephp/cakephp"

/*
 *  For CakePHP 4.0 and on, we retrieve the current controller object
 *  and are able to extract the controller name from that. We then
 *  retrieve the request object from the controller and are able to
 *  extract the action name from that. We then concatenate the two
 *  strings to form the transaction name.
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE`
 * This entails that the last wrapped call gets to name the txn.
 * No changes required to ensure OAPI compatibility this corresponds to the
 * default way of calling the wrapped function in func_end.
 *
 */
NR_PHP_WRAPPER(nr_cakephp_name_the_wt_4) {
  zval* this_var = 0;
  zval* czval = 0;
  char* controller = 0;
  char* action = 0;
  int clen = 0;
  int alen = 0;
  char* name = 0;
  zval* action_zval = NULL;
  zval* request = NULL;
  zval action_param;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_CAKEPHP);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: improper this");
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  czval = nr_php_get_zval_object_property(this_var, "name" TSRMLS_CC);
  if (0 == czval) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: this has no name");
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  if (!nr_php_is_zval_valid_string(czval)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: controller name is not a string");
  } else {
    clen = Z_STRLEN_P(czval);
    if (clen <= 0) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "CakePHP: controller name string is not long enough");
    } else {
      clen += sizeof("Controller");
      controller = (char*)nr_alloca(clen + 1);
      nr_strxcpy(controller, Z_STRVAL_P(czval), Z_STRLEN_P(czval));
      nr_strcat(controller, "Controller");
    }
  }

  NR_PHP_WRAPPER_CALL;

  request = nr_php_call(this_var, "getRequest");
  if (!nr_php_is_zval_valid_object(request)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: no request found in controller");
    goto end;
  }

  nr_php_zval_str(&action_param, "action");
  action_zval = nr_php_call(request, "getParam", &action_param);
  zval_dtor(&action_param);
  if (!nr_php_is_zval_non_empty_string(action_zval)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: no action param found in request");
    goto end;
  } else {
    alen = Z_STRLEN_P(action_zval);
    action = (char*)nr_alloca(alen + 1);
    nr_strxcpy(action, Z_STRVAL_P(action_zval), alen);
  }

  if ((0 == clen) && (0 == alen)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "CakePHP: nothing to call the transaction (yet?)");
    goto end;
  }

  name = (char*)nr_alloca(alen + clen + 2);
  if (clen) {
    nr_strcpy(name, controller);
  }
  if (alen) {
    if (clen) {
      nr_strcat(name, "/");
      nr_strcat(name, action);
    } else {
      nr_strcpy(name, action);
    }
  }

  nr_txn_set_path("CakePHP", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

end:
  nr_php_scope_release(&this_var);
  nr_php_zval_free(&request);
  nr_php_zval_free(&action_zval);
}
NR_PHP_WRAPPER_END

/*
 * CakePHP 4.0+
 *
 * Report errors and exceptions caught by CakePHP's error handler.
 */
NR_PHP_WRAPPER(nr_cakephp_error_handler_wrapper) {
  zval* exception = NULL;
  char* request_uri = nr_php_get_server_global("REQUEST_URI");

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_CAKEPHP);

  exception = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS);
  if (!nr_php_is_zval_valid_object(exception)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: exception is NULL or not an object",
                     __func__);
    goto end;
  }

  if (NR_SUCCESS
      != nr_php_error_record_exception(
          NRPRG(txn), exception, nr_php_error_get_priority(E_ERROR), true,
          "Uncaught exception ", &NRPRG(exception_filters))) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: unable to record exception", __func__);
  }

  if (NULL != request_uri) {
    nr_txn_set_path("CakePHP Exception", NRPRG(txn), request_uri, NR_PATH_TYPE_URI,
                    NR_OK_TO_OVERWRITE);
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: request uri is NULL", __func__);
  }

end:
  nr_php_arg_release(&exception);
  nr_free(request_uri);
}
NR_PHP_WRAPPER_END

/*
 * Enable CakePHP 4.0+
 */
void nr_cakephp_enable(TSRMLS_D) {
  nr_php_wrap_user_function(
      NR_PSTR("Cake\\Controller\\Controller::invokeAction"),
      nr_cakephp_name_the_wt_4);
  nr_php_wrap_user_function(
      NR_PSTR(
          "Cake\\Error\\Middleware\\ErrorHandlerMiddleware::handleException"),
      nr_cakephp_error_handler_wrapper);
  nr_txn_suggest_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                               PHP_PACKAGE_VERSION_UNKNOWN);
}
