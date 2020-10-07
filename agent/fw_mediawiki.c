/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Transaction naming for MediaWiki < 1.18.0.
 *
 * We trap calls to MediaWiki::setVal and look at the first argument. If it is
 * 'action' then the second argument is the action. This handles normal
 * requests and we name them "/action/$ACTION'. To provide a better user
 * experience we split out API calls and name them '/api/$FUNCTION'. This is
 * done by trapping ApiMain::__construct. This takes as its first argument a
 * WebRequest object. That object has an array called 'data'. That array will
 * contain a member named 'action'.
 */
NR_PHP_WRAPPER(nr_mediawiki_name_the_wt_non_api) {
  char* name = NULL;
  zval* arg1 = NULL;
  zval* arg2 = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MEDIAWIKI);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string(arg1)) {
    if (0 != nr_strncmp(Z_STRVAL_P(arg1), "action", Z_STRLEN_P(arg1))) {
      goto leave;
    }

    arg2 = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    /*
     * As PHP 7.0 will usually return an IS_REFERENCE zval here due to
     * MediaWiki::setVal() taking its second parameter by reference, we need to
     * dereference it. We do need to release the original arg2 since we bumped
     * its ref count, but we don't need to destroy the dereferenced zval because
     * we don't own it.
     */
    arg2 = nr_php_zval_dereference(arg2);

    if (nr_php_is_zval_non_empty_string(arg2)) {
      name = (char*)nr_alloca(Z_STRLEN_P(arg2) + 10);
      nr_strcpy(name, "action/");
      nr_strxcpy(name + 7, Z_STRVAL_P(arg2), Z_STRLEN_P(arg2));

      nr_txn_set_path("MediaWiki non-API", NRPRG(txn), name,
                      NR_PATH_TYPE_ACTION, NR_NOT_OK_TO_OVERWRITE);
    }
  }

leave:
  NR_PHP_WRAPPER_CALL;

  nr_php_arg_release(&arg1);
  nr_php_arg_release(&arg2);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_mediawiki_name_the_wt_api) {
  zval* data = NULL;
  zval* arg1 = NULL;
  char* name = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MEDIAWIKI);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_valid_object(arg1)) {
    zval* action = NULL;

    /*
     * arg1 is the WebRequest object. Extract its 'data' member, which should
     * be an array.
     */
    data = nr_php_get_zval_object_property(arg1, "data" TSRMLS_CC);
    if (NULL == data) {
      /*
       * We won't log here: MediaWiki 1.18.0 and later are instrumented a
       * different way below, so this is an expected failure. We'll just return
       * and let the ApiMain::setupExecuteAction() hook figure this out.
       */
      goto leave;
    }
    if (!nr_php_is_zval_valid_array(data)) {
      nrl_verbosedebug(NRL_FRAMEWORK, "MediaWiki: data not an array");
      goto leave;
    }

    /*
     * Now examine the data array looking for an element named 'action'. If
     * found then that is the name of the API call.
     */
    action = nr_php_zend_hash_find(Z_ARRVAL_P(data), "action");
    if (nr_php_is_zval_valid_string(action)) {
      name = (char*)nr_alloca(Z_STRLEN_P(action) + 5);
      nr_strcpy(name, "api/");
      nr_strxcpy(name + 4, Z_STRVAL_P(action), Z_STRLEN_P(action));

      nr_txn_set_path("MediaWiki_API", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                      NR_NOT_OK_TO_OVERWRITE);
    }
  }

leave:
  NR_PHP_WRAPPER_CALL;

  nr_php_arg_release(&arg1);
}
NR_PHP_WRAPPER_END

/*
 * Transaction naming for MediaWiki >= 1.18.0.
 *
 * MediaWiki uses MediaWiki::getAction() to ascertain what action is desired. A
 * set of actions such as "view" and "edit" are baked into MediaWiki, and
 * custom actions are supported by either adding a listener to the
 * UnknownAction hook (in 1.18 and older) or by adding to the $wgActions
 * global.
 */
NR_PHP_WRAPPER(nr_mediawiki_getaction) {
  char* name = NULL;
  zval** return_value = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MEDIAWIKI);

  return_value = nr_php_get_return_value_ptr(TSRMLS_C);

  NR_PHP_WRAPPER_CALL;

  if ((NULL == return_value)
      || (0 == nr_php_is_zval_non_empty_string(*return_value))) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: return value is invalid", __func__);
    goto leave;
  }

  name = nr_formatf("action/%.*s", NRSAFELEN(Z_STRLEN_P(*return_value)),
                    Z_STRVAL_P(*return_value));

  /*
   * Marking as OK to overwrite as the last action will be the one that's
   * processed (although in a normal request, there'll only be one action
   * anyway).
   */
  nr_txn_set_path("MediaWiki non-API", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

leave:
  nr_free(name);
}
NR_PHP_WRAPPER_END

/*
 * API transaction naming for MediaWiki >= 1.18.0.
 *
 * Unlike regular transactions, API transactions are funnelled through an
 * ApiMain object. The action name is kept in the mAction property on that
 * object, but that property isn't set until ApiMain::setupExecuteAction() is
 * called, so we'll wait until after that's done.
 */
NR_PHP_WRAPPER(nr_mediawiki_apimain_setupexecuteaction) {
  zval* action = NULL;
  char* name = NULL;
  zval* this_var = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MEDIAWIKI);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: $this is not an object", __func__);
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  NR_PHP_WRAPPER_CALL;

  action = nr_php_get_zval_object_property(this_var, "mAction" TSRMLS_CC);
  if (!nr_php_is_zval_non_empty_string(action)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: action is not a string", __func__);
    goto leave;
  }

  name = nr_formatf("api/%.*s", NRSAFELEN(Z_STRLEN_P(action)),
                    Z_STRVAL_P(action));

  nr_txn_set_path("MediaWiki_API", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

leave:
  nr_free(name);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Enable the MediaWiki instrumentation.
 */
void nr_mediawiki_enable(TSRMLS_D) {
  /*
   * Instrumentation for MediaWiki before version 1.18.0.
   */
  nr_php_wrap_user_function(NR_PSTR("MediaWiki::setVal"),
                            nr_mediawiki_name_the_wt_non_api TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("ApiMain::__construct"),
                            nr_mediawiki_name_the_wt_api TSRMLS_CC);

  /*
   * Instrumentation for MediaWiki 1.18.0 and later.
   */
  nr_php_wrap_user_function(NR_PSTR("MediaWiki::getAction"),
                            nr_mediawiki_getaction TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("ApiMain::setupExecuteAction"),
                            nr_mediawiki_apimain_setupexecuteaction TSRMLS_CC);
}
