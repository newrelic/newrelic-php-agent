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

NR_PHP_WRAPPER(nr_symfony1_controller_dispatch) {
  int prev_dispatch = NRPRG(symfony1_in_dispatch);

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY1);

  NRPRG(symfony1_in_dispatch) = 1;
  NR_PHP_WRAPPER_CALL;
  NRPRG(symfony1_in_dispatch) = prev_dispatch;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_symfony1_error404exception_printstacktrace) {
  int prev_error404 = NRPRG(symfony1_in_error404);

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY1);

  NRPRG(symfony1_in_error404) = 1;
  NR_PHP_WRAPPER_CALL;
  NRPRG(symfony1_in_error404) = prev_error404;
}
NR_PHP_WRAPPER_END

/*
 * Determine the Web Transaction name from the Symfony1 dispatcher.
 * Usage: called from a specific user function wrapper
 */
NR_PHP_WRAPPER(nr_symfony1_name_the_wt) {
  zval* module_name = 0;
  zval* action_name = 0;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY1);

  /*
   * We're looking for a particular active call stack:
   * 1. (php function) ...->dispatch (...)
   *   ..calls..
   * 2. (php function) ...->forward (module_name, action_name)  (This function
   * pre-call wrapped)
   *
   * This is to say we wrap the call to "forward", but are only sensitive to
   * that frame if it is called from dispatch. We track this via the
   * symfony1_in_dispatch global, which is set by the above
   * nr_symfony1_controller_dispatch() wrapper.
   */
  if (0 == NRPRG(symfony1_in_dispatch)) {
    nrl_debug(NRL_FRAMEWORK, "%s: forward() called, but not from dispatch()",
              __func__);
    NR_PHP_WRAPPER_LEAVE;
  }

  module_name = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  action_name = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (0 == nr_php_is_zval_non_empty_string(module_name)) {
    nrl_debug(NRL_FRAMEWORK, "Symfony1 module_name not a string");
    goto end;
  }

  if (0 == nr_php_is_zval_non_empty_string(action_name)) {
    nrl_debug(NRL_FRAMEWORK, "Symfony1 action_name not a string");
    goto end;
  }

  {
    char* name = nr_formatf("%.*s/%.*s", NRSAFELEN(Z_STRLEN_P(module_name)),
                            Z_STRVAL_P(module_name),
                            NRSAFELEN(Z_STRLEN_P(action_name)),
                            Z_STRVAL_P(action_name));
    nr_txn_assignment_t ok_to_override;

    /*
     * This bit of hackery is here for BC reasons. Prior to version 6.6 of the
     * agent, we always named Symfony 1 transactions based on the initially
     * resolved action. This allowed for MGIs due to the way Symfony 1 handles
     * 404 errors: it initially tries to synthesise the controller and action
     * from the request URL and routes based on that, then only handles the
     * routing error by forwarding after the 404 exception is thrown.
     *
     * The simple fix here is to name based on the final resolved action (after
     * all forwards are complete), which is what we do in Symfony 2/3, but
     * doing so will change the automatic transaction names for users who
     * forward to different controller actions. So instead we'll have an extra
     * check for whether Symfony is handling a 404: if so, then (and only then)
     * will we use the target of the forwarded transaction to name the
     * transaction.
     *
     * There is a very minor bit of cheese moving nevertheless: if the user
     * calls sfAction::forward() _within_ an action configured as the 404
     * handler, we'll now name on the last action rather than the first.
     */
    if (NRPRG(symfony1_in_error404)) {
      ok_to_override = NR_OK_TO_OVERWRITE;
    } else {
      ok_to_override = NR_NOT_OK_TO_OVERWRITE;
    }

    nr_txn_set_path("Symfony1", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    ok_to_override);

    nr_free(name);
  }

end:
  NR_PHP_WRAPPER_CALL;

  nr_php_arg_release(&module_name);
  nr_php_arg_release(&action_name);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_symfony1_context_loadfactories) {
  zval* controller;
  zval* name;
  zval* scope;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY1);

  scope = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /*
   * First, we need to actually call loadFactories(), since the information we
   * need is filled in by it.
   */
  NR_PHP_WRAPPER_CALL;

  /*
   * Now we need to get the controller class so we can wrap methods on it.
   * Effectively, we need to call $this->get('controller'). (Another option
   * would be to poke around in the $factories array, but as get() is the
   * public API, let's use that so we're not tied too deeply to implementation
   * details.)
   */
  name = nr_php_zval_alloc();
  nr_php_zval_str(name, "controller");
  controller = nr_php_call(scope, "get", name);
  if (nr_php_is_zval_valid_object(controller)) {
    const char* klass = nr_php_class_entry_name(Z_OBJCE_P(controller));
    char* method;

    method = nr_formatf("%s::dispatch", klass);
    nr_php_wrap_user_function(method, nr_strlen(method),
                              nr_symfony1_controller_dispatch TSRMLS_CC);
    nr_free(method);

    method = nr_formatf("%s::forward", klass);
    nr_php_wrap_user_function(method, nr_strlen(method),
                              nr_symfony1_name_the_wt TSRMLS_CC);
    nr_free(method);
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: the controller factory is not an object", __func__);
  }
  nr_php_zval_free(&controller);
  nr_php_zval_free(&name);

  nr_php_scope_release(&scope);
}
NR_PHP_WRAPPER_END

void nr_symfony1_enable(TSRMLS_D) {
  NRPRG(symfony1_in_dispatch) = 0;
  NRPRG(symfony1_in_error404) = 0;

  /*
   * We want to hook two methods on the controller class for naming purposes,
   * but it's possible for the user to override which class this is via
   * factories.yml. As a result, we'll hook the method that loads the factories
   * (which is always called as part of initialising the application), then
   * instrument once we know what the controller class is.
   */
  nr_php_wrap_user_function(NR_PSTR("sfContext::loadFactories"),
                            nr_symfony1_context_loadfactories TSRMLS_CC);

  nr_php_wrap_user_function(
      NR_PSTR("sfError404Exception::printStackTrace"),
      nr_symfony1_error404exception_printstacktrace TSRMLS_CC);
}
