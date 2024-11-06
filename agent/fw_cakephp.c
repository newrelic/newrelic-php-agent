/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_execute.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"

nr_framework_classification_t nr_cakephp_special_1(
    const char* filename TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (nr_strcaseidx(filename, "cake/libs/object.php") >= 0) {
    return FRAMEWORK_IS_SPECIAL;
  }

  return FRAMEWORK_IS_NORMAL;
}

nr_framework_classification_t nr_cakephp_special_2(
    const char* filename TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (nr_strcaseidx(filename, "cake/core/app.php") >= 0) {
    return FRAMEWORK_IS_SPECIAL;
  }

  return FRAMEWORK_IS_NORMAL;
}

/*
 * For CakePHP 1.2 and 1.3 (and possibly earlier versions too) we hook into
 * Component::initialize(). This function takes a controller as a parameter
 * and we look into the params array of that controller object, and pick up
 * the controller and action out of that array.
 *
 * CakePHP 1.x is end-of-life and no longer supported by the agent.
 * Cake PHP 1.x does not support PHP 8+ and this wrapper is not updated for OAPI
 * compatibility.
 *
 */
NR_PHP_WRAPPER(nr_cakephp_name_the_wt_pre20) {
  zval* arg1 = 0;
  zval* params;
  zval* czval;
  zval* azval;
  char* controller = 0;
  char* action = 0;
  int clen = 0;
  int alen = 0;
  char* name;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_CAKEPHP);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(arg1)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  params = nr_php_get_zval_object_property(arg1, "params" TSRMLS_CC);
  if (0 == params) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: no params found in component");
    goto end;
  }

  if (IS_ARRAY != Z_TYPE_P(params)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "CakePHP: component params is not an array");
    goto end;
  }

  czval = nr_php_get_zval_object_property(params, "controller" TSRMLS_CC);
  if (0 == czval) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "CakePHP: no params['controller'] in component");
  } else {
    clen = Z_STRLEN_P(czval);
    controller = (char*)nr_alloca(clen + 1);
    nr_strxcpy(controller, Z_STRVAL_P(czval), clen);
  }

  azval = nr_php_get_zval_object_property(params, "action" TSRMLS_CC);
  if (0 == azval) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "CakePHP: no params['action'] in component");
  } else {
    alen = Z_STRLEN_P(azval);
    action = (char*)nr_alloca(alen + 1);
    nr_strxcpy(action, Z_STRVAL_P(azval), alen);
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
  nr_php_arg_release(&arg1);
}
NR_PHP_WRAPPER_END

/*
 * For CakePHP 2.0 and on, we do things a little differently as the params
 * array doesn't exist in the component any more. Instead we hook the
 * Controller's invokeAction method. This gets the request as a parameter
 * and we get the action from the params array in that object. The
 * controller object ($this) has a name, and that name is used (along
 * with the word "Controller" appended which is what the CakePHP code does).
 *
 * CakePHP 2.x is end-of-life and in maintenance mode (critical bugfixes only).
 * As such, functionality added in PHP 7.1+ is not well supported.
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE`
 * This entails that the last wrapped call gets to name the txn.
 * No changes required to ensure OAPI compatibility this corresponds to the
 * default way of calling the wrapped function in func_end.
 *
 */
NR_PHP_WRAPPER(nr_cakephp_name_the_wt_2) {
  zval* arg1 = 0;
  zval* this_var = 0;
  zval* czval = 0;
  char* controller = 0;
  char* action = 0;
  int clen = 0;
  int alen = 0;
  char* name = 0;
  zval* params;
  zval* azval;

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

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(arg1)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  params = nr_php_get_zval_object_property(arg1, "params" TSRMLS_CC);
  if (0 == params) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: no params found in request");
    goto end;
  }

  if (IS_ARRAY != Z_TYPE_P(params)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: request params is not an array");
    goto end;
  }

  azval = nr_php_get_zval_object_property(params, "action" TSRMLS_CC);
  if (0 == azval) {
    nrl_verbosedebug(NRL_FRAMEWORK, "CakePHP: no params['action'] in request");
  } else {
    if (!nr_php_is_zval_valid_string(azval)) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "CakePHP: no string-valued params['action'] in request");
    } else {
      alen = Z_STRLEN_P(azval);
      action = (char*)nr_alloca(alen + 1);
      nr_strxcpy(action, Z_STRVAL_P(azval), alen);
    }
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
  nr_php_arg_release(&arg1);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * CakePHP 1.2, 1.3
 *
 * Dispatch::cakeError will be called if there is a problem during dispatch
 * (action or controller not found).
 *
 * CakePHP 1.x is end-of-life and no longer supported by the agent.
 * Cake PHP 1.x does not support PHP 8+ and this wrapper is not updated for OAPI
 * compatibility.
 *
 */
NR_PHP_WRAPPER(nr_cakephp_problem_1) {
  const char* name = "Dispatcher::cakeError";

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_CAKEPHP);

  nr_txn_set_path("CakePHP", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * CakePHP 2.0+
 *
 * If the action or controller is not found during the dispatch process, the
 * appropriate Exception will be created and thrown. We wrap the CakeException
 * constructor instead of the Exception handler, since CakePHP allows for the
 * handler to be completely replaced.
 *
 *  CakePHP 2.x is end-of-life and in maintenance mode (critical bugfixes only).
 * As such, functionality added in PHP 7.1+ is not well supported.
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_begin it needs to be explicitly set as a before_callback to
 * ensure OAPI compatibility. This entails that the first wrapped call gets to
 * name the txn.
 */
NR_PHP_WRAPPER(nr_cakephp_problem_2) {
  const char* name = "Exception";

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_CAKEPHP);

  nr_txn_set_path("CakePHP", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * Enable CakePHP 1.2, 1.3
 */
void nr_cakephp_enable_1(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("Component::initialize"),
                            nr_cakephp_name_the_wt_pre20 TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("Dispatcher::cakeError"),
                            nr_cakephp_problem_1 TSRMLS_CC);
}

/*
 * Enable CakePHP 2.0+
 */
void nr_cakephp_enable_2(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("Controller::invokeAction"),
                            nr_cakephp_name_the_wt_2 TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("CakeException::__construct"), nr_cakephp_problem_2, NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("CakeException::__construct"),
                            nr_cakephp_problem_2 TSRMLS_CC);
#endif
}
