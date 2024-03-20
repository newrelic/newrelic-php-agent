/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_error.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "fw_symfony_common.h"

NR_PHP_WRAPPER(nr_symfony4_exception) {
  int priority = nr_php_error_get_priority(E_ERROR);
  zval* event = NULL;
  zval* exception = NULL;

  /* Warning avoidance */
  (void)wraprec;

  /* Verify that we are using symfony 4, otherwise bail. */
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY4);

  if (NR_SUCCESS != nr_txn_record_error_worthy(NRPRG(txn), priority)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  /* Get the event that was given. */
  event = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /* Call the original function. */
  NR_PHP_WRAPPER_CALL;

  if (0 == nr_php_is_zval_valid_object(event)) {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 4: KernelEvent::onKernelException() does not "
                     "have an event parameter");
    goto end;
  }

  /*
   * Get the exception from the event.
   * Firstly, we check if ExceptionEvent is available - if yes, that means we
   * are using Symfony 5
   */
  exception = nr_php_call(event, "getThrowable");
  if (!nr_php_is_zval_valid_object(exception)) {
    exception = nr_php_call(event, "getException");
  }

  if (!nr_php_is_zval_valid_object(exception)) {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 4: getException() returned a non-object");
    goto end;
  }

  if (NR_SUCCESS
      != nr_php_error_record_exception(NRPRG(txn), exception, priority, true, NULL,
                                       &NRPRG(exception_filters) TSRMLS_CC)) {
    nrl_verbosedebug(NRL_TXN, "Symfony 4: unable to record exception");
  }

end:
  nr_php_arg_release(&event);
  nr_php_zval_free(&exception);
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This entails that the last wrapped call gets to name the
 * txn but it is overwritable if another better name comes along.
 */
NR_PHP_WRAPPER(nr_symfony4_console_application_run) {
  zval* command = NULL;
  zval* input = NULL;

  (void)wraprec;

  /* Verify that we are using symfony 4, otherwise bail. */
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY4);

  /*
   * The first parameter to this method should be an instance of an
   * InputInterface, which defines a method called getFirstArgument which will
   * return the command name, or an empty string if no command name was given.
   * We can then use that to name the transaction.
   */
  input = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_object_instanceof_class(
          input,
          "Symfony\\Component\\Console\\Input\\InputInterface" TSRMLS_CC)) {
    goto leave;
  }

  command = nr_php_call(input, "getFirstArgument");
  if (nr_php_is_zval_non_empty_string(command)) {
    nr_txn_set_path("Symfony4", NRPRG(txn), Z_STRVAL_P(command),
                    NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
  } else {
    /*
     * Not having any arguments will result in the same behaviour as
     * "bin/console list", so we'll name the transaction accordingly.
     */
    nr_txn_set_path("Symfony4", NRPRG(txn), "list", NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&input);
  nr_php_zval_free(&command);
}
NR_PHP_WRAPPER_END

/*
 *  * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.  See more naming logic
 * details within the function.
 */
NR_PHP_WRAPPER(nr_symfony4_name_the_wt) {
  zval* event = NULL;
  zval* request = NULL;

  /* Warning avoidance */
  (void)wraprec;

  /* Verify that we are using symfony 4, otherwise bail. */
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY4);

  /*
   * A high level overview of the logic:
   *
   * RouterListener::onKernelRequest() receives a GetResponseEvent (RequestEvent
   * in Symfony 5) parameter, which includes the request object accessible via
   * the getRequest() method. We want to get the request, then access its
   * attributes: the request matcher will create a number of internal attributes
   * prefixed by underscores as part of resolving the controller action.
   *
   * If the user has given their action method a friendly name via an
   * annotation or controller option, then this is available in _route. This is
   * likely to be shorter and clearer than the auto-generated controller
   * method, so it's the first preference.
   *
   * If _route doesn't exist, then _controller should always exist. For
   * non-subrequests, this will be a name Symfony generates from the fully
   * qualified class name and method. For subrequests, this is whatever the
   * user gave Controller::forward(), which will hopefully be more or less the
   * same thing.
   */

  event = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(event)) {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 4: RouterListener::onKernelRequest() does not "
                     "have an event parameter");
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  /* Get the request object from the event. */
  request = nr_php_call(event, "getRequest");
  if (nr_php_object_instanceof_class(
          request, "Symfony\\Component\\HttpFoundation\\Request" TSRMLS_CC)) {
    /* Let's look for _route first. */
    zval* route_rval
        = nr_symfony_object_get_string(request, "_route" TSRMLS_CC);

    if (route_rval) {
      if (NR_SUCCESS
          != nr_symfony_name_the_wt_from_zval(route_rval TSRMLS_CC,
                                              "Symfony 4")) {
        nrl_verbosedebug(
            NRL_TXN, "Symfony 4: Request::get('_route') returned a non-string");
      }
      nr_php_zval_free(&route_rval);
    } else {
      /* No _route. Look for _controller. */
      zval* controller_rval
          = nr_symfony_object_get_string(request, "_controller" TSRMLS_CC);

      if (controller_rval) {
        if (NR_SUCCESS
            != nr_symfony_name_the_wt_from_zval(controller_rval TSRMLS_CC,
                                                "Symfony 4")) {
          nrl_verbosedebug(
              NRL_TXN,
              "Symfony 4: Request::get('_controller') returned a non-string");
        }
        nr_php_zval_free(&controller_rval);
      } else {
        nrl_verbosedebug(NRL_TXN,
                         "Symfony 4: Neither _controller nor _route is set");
      }
    }
  } else {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 4: GetResponseEvent::getRequest() returned a "
                     "non-Request object");
  }

end:
  nr_php_arg_release(&event);
  nr_php_zval_free(&request);
}
NR_PHP_WRAPPER_END

void nr_symfony4_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent having to name routing errors.
   */
  nr_txn_set_path("Symfony4", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  /*
   * Originally, we had a pre-callback hook on HttpKernel::filterResponse().
   * This works fine for simple requests, but fails on subrequests forwarded by
   * Controller::forward() due to HttpKernel::filterResponse() being called in
   * the reverse order as Symfony unwinds the request stack, which means we get
   * the initial request name rather than the innermost, which is what we want.
   *
   * In practice, where we really want to hook in is about two lines into
   * HttpKernel::handleRaw(), but that's rather difficult given our API, so
   * instead we'll hook into the RouterListener. Once onKernelRequest() has
   * finished its work, the controller has been resolved, so we can go from
   * there. This is reliable as long as the user hasn't replaced the router
   * listener service, which is a pretty deep customisation: chances are a user
   * who's doing that is quite capable of naming a transaction by hand.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Symfony\\Component\\HttpKernel\\"
              "EventListener\\RouterListener::onKernelRequest"),
      nr_symfony4_name_the_wt TSRMLS_CC);

  /*
   * Symfony does a pretty good job of catching errors but that means we
   * don't register them as errors in the UI. They just show up as transactions.
   * In order to fix this, we hook into onKernelException() and
   * nr_txn_record_error and pass the exception message. Now we get errors in
   * the error analytics page.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Symfony\\Component\\HttpKernel\\"
              "EventListener\\ExceptionListener::onKernelException"),
      nr_symfony4_exception TSRMLS_CC);

  /*
   * In Symfony 5 listener that catch errors was changed to ErrorListener,
   * we try to hook into it
   */
  nr_php_wrap_user_function(
      NR_PSTR("Symfony\\Component\\HttpKernel\\"
              "EventListener\\ErrorListener::onKernelException"),
      nr_symfony4_exception TSRMLS_CC);

  /*
   * Listen for Symfony commands so we can name those appropriately.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Symfony\\Component\\Console\\Command\\Command::run"),
      nr_symfony4_console_application_run, NULL);
#else
  nr_php_wrap_user_function(
      NR_PSTR("Symfony\\Component\\Console\\Command\\Command::run"),
      nr_symfony4_console_application_run TSRMLS_CC);
#endif

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "symfony/http-kernel",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
