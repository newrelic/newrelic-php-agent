/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Yii1: Set the web transaction name from the controllerId + actionId combo.
 *
 * * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_begin it needs to be explicitly set as a before_callback to
 * ensure OAPI compatibility. This entails that the first wrapped call gets to
 * name the txn.
 */
NR_PHP_WRAPPER(nr_yii1_runWithParams_wrapper) {
  zval* classz = NULL;
  zval* idz = NULL;
  zval* this_var = NULL;
  const char* class_name = NULL;
  nr_string_len_t class_name_length;
  char* id_name = NULL;
  nr_string_len_t id_name_length;
  char* buf = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_YII1);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == this_var) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Yii: improper this");
    goto end;
  }

  classz = nr_php_call(this_var, "getController");
  if (!nr_php_is_zval_valid_object(classz)) {
    nrl_warning(NRL_FRAMEWORK, "getController does not return an object (%d)",
                Z_TYPE_P(classz));
  } else {
    class_name = nr_php_class_entry_name(Z_OBJCE_P(classz));
    class_name_length = nr_php_class_entry_name_length(Z_OBJCE_P(classz));

    idz = nr_php_call(this_var, "getId");
    if (!nr_php_is_zval_valid_string(idz)) {
      nrl_warning(NRL_FRAMEWORK, "getId does not return a string (%d)",
                  Z_TYPE_P(idz));
    } else {
      id_name = Z_STRVAL_P(idz);
      id_name_length = Z_STRLEN_P(idz);

      if (class_name_length + id_name_length > 256) {
        nrl_warning(
            NRL_FRAMEWORK,
            "Yii class and id names are too long (> %d); Yii naming not used",
            256);
      } else {
        buf = (char*)nr_alloca(class_name_length + id_name_length + 2);
        nr_strxcpy(buf, class_name, class_name_length);
        nr_strxcpy(buf + class_name_length, "/", 1);
        nr_strxcpy(buf + class_name_length + 1, id_name, id_name_length);

        nr_txn_set_path("Yii", NRPRG(txn), buf, NR_PATH_TYPE_ACTION,
                        NR_NOT_OK_TO_OVERWRITE);
      }
    }
    nr_php_zval_free(&idz);
  }
  nr_php_zval_free(&classz);
end:
  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Enable Yii1 instrumentation.
 */
void nr_yii1_enable(TSRMLS_D) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("CAction::runWithParams"), nr_yii1_runWithParams_wrapper, NULL,
      NULL);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("CInlineAction::runWithParams"), nr_yii1_runWithParams_wrapper,
      NULL, NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("CAction::runWithParams"),
                            nr_yii1_runWithParams_wrapper TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("CInlineAction::runWithParams"),
                            nr_yii1_runWithParams_wrapper TSRMLS_CC);
#endif
}

/*
 * Yii2: Set the web transaction name from the unique action ID.
 */
NR_PHP_WRAPPER(nr_yii2_runWithParams_wrapper) {
  zval* this_var = NULL;
  zval* unique_idz = NULL;
  const char* unique_id = NULL;
  nr_string_len_t unique_id_length;
  char* transaction_name = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_YII2);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == this_var) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Yii2: improper this");
    goto end;
  }

  unique_idz = nr_php_call(this_var, "getUniqueId");
  if (nr_php_is_zval_valid_string(unique_idz)) {
    unique_id = Z_STRVAL_P(unique_idz);
    unique_id_length = Z_STRLEN_P(unique_idz);

    if (unique_id_length > 256) {
      nrl_warning(NRL_FRAMEWORK,
                  "Yii2 unique ID is too long (> %d); Yii2 naming not used",
                  256);
    } else {
      transaction_name = (char*)nr_alloca(unique_id_length + 1);
      nr_strxcpy(transaction_name, unique_id, unique_id_length);

      nr_txn_set_path("Yii2", NRPRG(txn), transaction_name, NR_PATH_TYPE_ACTION,
                      NR_NOT_OK_TO_OVERWRITE);
    }
  }
  nr_php_zval_free(&unique_idz);
end:
  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Yii2: Report errors and exceptions when built-in ErrorHandler is enabled.
 */
NR_PHP_WRAPPER(nr_yii2_error_handler_wrapper) {
  zval* exception = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_YII2);

  exception = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == exception || !nr_php_is_zval_valid_object(exception)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: exception is NULL or not an object",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  if (NR_SUCCESS
      != nr_php_error_record_exception(
          NRPRG(txn), exception, nr_php_error_get_priority(E_ERROR), true,
          "Uncaught exception ", &NRPRG(exception_filters) TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: unable to record exception", __func__);
  }

end:
  nr_php_arg_release(&exception);
}
NR_PHP_WRAPPER_END

/*
 * Enable Yii2 instrumentation.
 */
void nr_yii2_enable(TSRMLS_D) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("yii\\base\\Action::runWithParams"),
      nr_yii2_runWithParams_wrapper, NULL, NULL);
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("yii\\base\\InlineAction::runWithParams"),
      nr_yii2_runWithParams_wrapper, NULL, NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("yii\\base\\Action::runWithParams"),
                            nr_yii2_runWithParams_wrapper TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("yii\\base\\InlineAction::runWithParams"),
                            nr_yii2_runWithParams_wrapper TSRMLS_CC);
#endif
  /*
   * Wrap Yii2 global error and exception handling methods.
   * Given that: ErrorHandler::handleException(), ::handleError() and
   * ::handleFatalError() all call ::logException($exception) at the right time,
   * we will wrap this one to cover all cases.
   * @see
   * https://github.com/yiisoft/yii2/blob/master/framework/base/ErrorHandler.php
   *
   * Note: one can also set YII_ENABLE_ERROR_HANDLER constant to FALSE, this way
   * allowing default PHP error handler to be intercepted by the NewRelic agent
   * implementation.
   */
  nr_php_wrap_user_function(NR_PSTR("yii\\base\\ErrorHandler::logException"),
                            nr_yii2_error_handler_wrapper TSRMLS_CC);
}
