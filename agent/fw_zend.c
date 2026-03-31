/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "lib_zend_http.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * How ZF1 Routing Works
 * =====================
 *
 * In a standard ZF1 application, requests enter the front controller
 * (Zend_Controller_Front) where a route is selected based on the request URL.
 * Once a route has been found, the front controller then enters the dispatch
 * loop where it will determine which controller(s) and action(s) to invoke. In
 * most cases, this is the controller and action associated with the route.
 * However, invoking a different action, or even multiple actions is a normal
 * practice in Zend applications. For example, to forward a request to another
 * controller, or redirect to another URL. Zend also provides a plugin interface
 * to isolate cross-cutting routing concerns into separate classes. For example,
 * implementing authentication. The following pseudo-php code demonstrates the
 * core logic.
 *
 *   ```
 *   Zend_Application::run()
 *     Zend_Application_Bootstrap::run()
 *       Zend_Controller_Front::dispatch($request, $response)
 *         $request  = new Zend_Controller_Request_Http()  if $request  == NULL
 *         $response = new Zend_Controller_Response_Http() if $response == NULL
 *
 *         $plugins->routeStartup()
 *         $router->route($request)
 *         $plugins->routeShutdown()
 *
 *         $plugins->dispatchLoopStartup()
 *
 *         until $request->isDispatched()
 *           $request->setDispatched(true)
 *           $plugins->preDispatch($request)
 *           if $request->isDispatched()
 *             $dispatcher->dispatch($request, $response)
 *             $plugins->postDispatch($request)
 *
 *         $plugins->dispatchLoopShutdown()
 *  ```
 *
 * Ideally, we would hook `dispatchLoopShutdown()` and thereby wait until after
 * the final controller and action were selected and invoked to name the
 * transaction. There are two complications that prevent us from doing so.
 *
 * 1. An action or plugin can end the request early by calling the `exit()`
 *    function. If this occurs during the dispatch loop,
 * `dispatchLoopShutdown()` will never be invoked. There are at least three
 * standard Zend components that do this: Redirect, Json, and AutoComplete.
 * 2. An exception can be thrown at any time.
 *
 * To address early exits, we also hook `preDispatch()`. This ensures we
 * have a chance to name the transaction when an early exit occurs, at the
 * cost of redundantly setting the transaction name in each hook otherwise.
 *
 * We explicitly choose not to try and cope with exceptions. The default
 * behavior of Zend is it catch exceptions that occur during the dispatch
 * loop and record it within the response.
 */

/*
 * Purpose : Name the transaction based on the current controller and action.
 *
 * Params  : 1. A Zend_Controller_Request_Abstract object.
 *
 * Returns : Nothing.
 */
static void nr_zend_name_the_wt(zval* request TSRMLS_DC) {
  zval* module = NULL;
  zval* controller = NULL;
  zval* action = NULL;
  char buf[512];

  if (NULL == request) {
    return;
  }

  if ((0 == nr_php_object_has_method(request, "getModuleName" TSRMLS_CC))
      || (0 == nr_php_object_has_method(request, "getControllerName" TSRMLS_CC))
      || (0 == nr_php_object_has_method(request, "getActionName" TSRMLS_CC))) {
    return;
  }

  module = nr_php_call(request, "getModuleName");
  controller = nr_php_call(request, "getControllerName");
  action = nr_php_call(request, "getActionName");

  if (module || controller || action) {
    buf[0] = '\0';
    snprintf(
        buf, sizeof(buf), "%.*s/%.*s/%.*s",
        nr_php_is_zval_non_empty_string(module) ? NRSAFELEN(Z_STRLEN_P(module))
                                                : 32,
        nr_php_is_zval_non_empty_string(module) ? Z_STRVAL_P(module)
                                                : "NoModule",

        nr_php_is_zval_non_empty_string(controller)
            ? NRSAFELEN(Z_STRLEN_P(controller))
            : 32,
        nr_php_is_zval_non_empty_string(controller) ? Z_STRVAL_P(controller)
                                                    : "NoController",

        nr_php_is_zval_non_empty_string(action) ? NRSAFELEN(Z_STRLEN_P(action))
                                                : 32,
        nr_php_is_zval_non_empty_string(action) ? Z_STRVAL_P(action)
                                                : "NoAction");

    nr_txn_set_path("Zend", NRPRG(txn), buf, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }

  nr_php_zval_free(&module);
  nr_php_zval_free(&controller);
  nr_php_zval_free(&action);
}

/*
 * Purpose : Invoke Zend_Controller_Plugin_Broker::getRequest().
 *
 * Params  : 1. A Zend_Controller_Plugin_Broker object.
 *
 * Returns : A pointer to a zval, which is the return value of the function,
 *           if successful; otherwise, NULL. The caller is responsible
 *           for destroying the return value.
 */
static zval* nr_zend_plugin_broker_get_request(zval* plugins TSRMLS_DC) {
  zval* request = NULL;

  if (NULL == plugins) {
    return NULL;
  }

  request = nr_php_call(plugins, "getRequest");
  if (!nr_php_is_zval_valid_object(request)) {
    nr_php_zval_free(&request);
    return NULL;
  }

  return request;
}

/*
 * Purpose : Wrap Zend_Controller_Plugin_Broker::preDispatch(request) to
 *           try and set the transaction name as soon as the final controller
 *           and action have been determined. This is to ensure we name the
 *           transaction even if an early exit occurs.
 */
NR_PHP_WRAPPER(nr_zend_plugin_broker_pre_dispatch) {
  zval* this_var = NULL;
  zval* request = NULL;
  zval* dispatched = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_ZEND);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(this_var)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  request = nr_zend_plugin_broker_get_request(this_var TSRMLS_CC);
  if (NULL == request) {
    goto end;
  }

  /* isDispatched() returns true when the final controller and action are found.
   */
  dispatched = nr_php_call(request, "isDispatched");
  if (nr_php_is_zval_true(dispatched)) {
    nr_zend_name_the_wt(request TSRMLS_CC);
  }

  nr_php_zval_free(&dispatched);
  nr_php_zval_free(&request);

end:
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Wrap Zend_Controller_Plugin_Broker::dispatchLoopShutdown() to
 *           ensure the transaction name reflects the final controller and
 *           action.
 *
 * Returns : Nothing.
 */
NR_PHP_WRAPPER(nr_zend_plugin_broker_dispatch_loop_shutdown) {
  zval* this_var = NULL;
  zval* request = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_ZEND);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(this_var)) {
    goto end;
  }

  request = nr_zend_plugin_broker_get_request(this_var TSRMLS_CC);
  if (NULL != request) {
    nr_zend_name_the_wt(request TSRMLS_CC);
    nr_php_zval_free(&request);
  }

end:
  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_zend_enable(TSRMLS_D) {
  nr_php_wrap_user_function(
      NR_PSTR("Zend_Controller_Plugin_Broker::dispatchLoopShutdown"),
      nr_zend_plugin_broker_dispatch_loop_shutdown TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Zend_Controller_Plugin_Broker::preDispatch"),
      nr_zend_plugin_broker_pre_dispatch TSRMLS_CC);
}
