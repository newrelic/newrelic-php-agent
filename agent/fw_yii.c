/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Set the web transaction name from the action.
 *
 * * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_begin it needs to be explicitly set as a before_callback to
 * ensure OAPI compatibility. This entails that the first wrapped call gets to
 * name the txn.
 */

NR_PHP_WRAPPER(nr_yii_runWithParams_wrapper) {
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

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_YII);

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
  }

end:
  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Enable Yii instrumentation.
 */
void nr_yii_enable(TSRMLS_D) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("CAction::runWithParams"), nr_yii_runWithParams_wrapper, NULL);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("CInlineAction::runWithParams"), nr_yii_runWithParams_wrapper,
      NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("CAction::runWithParams"),
                            nr_yii_runWithParams_wrapper TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("CInlineAction::runWithParams"),
                            nr_yii_runWithParams_wrapper TSRMLS_CC);
#endif
}
