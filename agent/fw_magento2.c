/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_wrapper.h"
#include "php_hash.h"
#include "fw_magento_common.h"
#include "fw_hooks.h"
#include "nr_txn.h"
#include "util_logging.h"

/*
 * Magento 2 adds (depending on how you count)
 * either three or four completely separate routing paths in the Community
 * Edition.
 *
 * Normal routing looks broadly similar to Magento 1. It goes through a front
 * controller, eventually resolves to an Action object, and we can hook that and
 * name from there.
 *
 * Magento 2 beefed up Magento's caching support, and
 * this results in it shipping out of the box with a
 * page cache implementation that bypasses full action resolution. We need to
 * instrument that so that we can call out cached pages separately for timing
 * purposes.
 *
 * Additionally, Magento 2 has a concept of "plugins",
 * or "interceptors". (The names are used somewhat interchangably.) These are
 * classes that modify service classes on the fly: when installed and enabled,
 * they can request that service method(s) be rewritten on the fly before the DI
 * container returns the service object.
 *
 * Magento 2 uses this functionality to a significant extent within its core,
 * including in its implementation of REST and SOAP web services. These plugins
 * are invoked when the Magento\Framework\App\FrontControllerInterface service
 * is requested: they rewrite the service's dispatch() method, and if a REST or
 * SOAP web service is being requested, they replace the normal routing logic
 * with their own. We'll take the class of the returned object as an initial
 * name, since it provides more information than "unknown", but in most cases we
 * can do better.
 *
 * For REST requests, this results in the transaction being routed through an
 * "input parameter resolver" in the Magento\Webapi\Controller\Rest namespace.
 * We hook the resolve method to ensure we catch the request before it's
 * authorized, then call that resolver's getRoute() method to get a REST-
 * specific route object with plausible names.
 *
 * For SOAP requests, there are three subcases: listing the available WSDL
 * endpoints, handling a WSDL endpoint, and handling a SOAP request. For the
 * first two cases, we'll look at the internal helper methods that determine
 * whether those cases are dispatched. For the final case, we'll hook the SOAP
 * handler directly and capture the service class and method from there.
 */

/*
 * Purpose : Check if the transaction is still named "unknown", which occurs in
 *           nr_magento2_enable() as a fallback.
 *
 * Params  : 1. The transaction.
 *
 * Returns : Non-zero if the transaction is named "unknown"; zero otherwise.
 */
static int nr_magento2_is_txn_path_unknown(nrtxn_t* txn) {
  if (NR_PATH_TYPE_ACTION != txn->status.path_type) {
    return 0;
  }

  return (0 == nr_strcmp(txn->path, "unknown"));
}

/*
 * Purpose : Name the transaction from the given module prefix, service class
 *           name, and service method name. This is a pattern common to both the
 *           REST and SOAP modules.
 *
 * Params  : 1. The module prefix.
 *           2. The service class name as a zval.
 *           3. The service method name as a zval.
 */
static void nr_magento2_name_transaction_from_service(const char* module,
                                                      const zval* svc_class,
                                                      const zval* svc_method
                                                          TSRMLS_DC) {
  const char* klass = "NoController";
  const char* method = "NoAction";
  char* name = NULL;

  if (nr_php_is_zval_valid_string(svc_class)) {
    klass = Z_STRVAL_P(svc_class);
  }

  if (nr_php_is_zval_valid_string(svc_method)) {
    method = Z_STRVAL_P(svc_method);
  }

  name = nr_formatf("%s/%s/%s", module, klass, method);
  nr_txn_set_path("Magento", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  nr_free(name);
}

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This entails that the last wrapped call gets to name the
 * txn but it is overwritable if another better name comes along.
 */
NR_PHP_WRAPPER(nr_magento2_action_dispatch) {
  zval* this_var = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  nr_magento_name_transaction(this_var TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_pagecache_kernel_load) {
  const char* name = "page_cache";
  zval** response = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  response = NR_GET_RETURN_VALUE_PTR;

  NR_PHP_WRAPPER_CALL;

  /*
   * Magento\Framework\App\PageCache\Kernel::load returns a
   * Magento\Framework\App\Response\Http if there is a cache hit and false
   * otherwise.
   */
  if (response && nr_php_is_zval_valid_object(*response)) {
    nr_txn_set_path("Magento", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_objectmanager_get) {
  const char* fci_class = "Magento\\Framework\\App\\FrontControllerInterface";
  zval** retval_ptr = NULL;
  zval* type = NULL;
  (void)wraprec;

  /*
   * First, check if the caller is even requesting a front controller.
   */
  type = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_string(type)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: object type is not a string",
                     __func__);
    goto leave;
  } else if (-1
             == nr_strncaseidx(Z_STRVAL_P(type), fci_class,
                               NRSAFELEN(Z_STRLEN_P(type)))) {
    /*
     * Not requesting a FrontControllerInterface; exit gracefully.
     */
    goto leave;
  }
  retval_ptr = NR_GET_RETURN_VALUE_PTR;
  NR_PHP_WRAPPER_CALL;

  if ((NULL == retval_ptr) || !nr_php_is_zval_valid_object(*retval_ptr)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: return value is not an object",
                     __func__);
    goto leave;
  }

  /*
   * Initial naming: no routing should have occurred yet, but we'll name the
   * transaction after the returned class so that if it's a third party
   * interceptor that we don't handle, there's at least something more useful
   * than "unknown".
   *
   * We can't just use NR_NOT_OK_TO_OVERWRITE because the enable function has
   * already set a NR_PATH_TYPE_ACTION path, so instead we have to check if the
   * path name is "unknown" and go from there.
   */
  if (nr_magento2_is_txn_path_unknown(NRPRG(txn))) {
    char* name = nr_formatf("FrontController/%s",
                            nr_php_class_entry_name(Z_OBJCE_P(*retval_ptr)));

    nr_txn_set_path("Magento", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);

    nr_free(name);
  }

leave:
  nr_php_arg_release(&type);
}
NR_PHP_WRAPPER_END

/*
 *  * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This combination entails that the last wrapped function
 * call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_inputparamsresolver_resolve) {
  const char* this_klass
      = "Magento\\Webapi\\Controller\\Rest\\InputParamsResolver";
  const char* route_klass = "Magento\\Webapi\\Controller\\Rest\\Router\\Route";
  zval* this_var = NULL;
  zval* route = NULL;
  zval* svc_class = NULL;
  zval* svc_method = NULL;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if ((NULL == this_var)
      || !nr_php_object_instanceof_class(this_var, this_klass TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: resolver is not %s", __func__,
                     this_klass);
    goto leave;
  }

  route = nr_php_call(this_var, "getRoute");
  if ((NULL == route)
      || !nr_php_object_instanceof_class(route, route_klass TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: route is not %s", __func__,
                     route_klass);
    goto leave;
  }

  svc_class = nr_php_call(route, "getServiceClass");
  svc_method = nr_php_call(route, "getServiceMethod");

  nr_magento2_name_transaction_from_service("Webapi/Rest", svc_class,
                                            svc_method TSRMLS_CC);

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_scope_release(&this_var);
  nr_php_zval_free(&route);
  nr_php_zval_free(&svc_class);
  nr_php_zval_free(&svc_method);
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure as it will use the default func_end
 * after callback. OAPI compatibility. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_soap_iswsdlrequest) {
  zval** retval_ptr = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  retval_ptr = NR_GET_RETURN_VALUE_PTR;
  NR_PHP_WRAPPER_CALL;

  if (retval_ptr && nr_php_is_zval_true(*retval_ptr)) {
    nr_txn_set_path("Magento", NRPRG(txn), "Webapi/Soap/Wsdl",
                    NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
  }
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure as it will use the default func_end
 * after callback. OAPI compatibility. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_soap_iswsdllistrequest) {
  zval** retval_ptr = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  retval_ptr = NR_GET_RETURN_VALUE_PTR;
  NR_PHP_WRAPPER_CALL;

  if (retval_ptr && nr_php_is_zval_true(*retval_ptr)) {
    nr_txn_set_path("Magento", NRPRG(txn), "Webapi/Soap/WsdlList",
                    NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
  }
}
NR_PHP_WRAPPER_END

/*
 * Takes the following arguments:
 * string $serviceClass
 * string $serviceMethod
 * array $arguments
 *
 *
 *  txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This combination entails that the last wrapped function
 * call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_soap_handler_preparerequestdata) {
  zval* svc_class = NULL;
  zval* svc_method = NULL;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  svc_class = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  svc_method = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  nr_magento2_name_transaction_from_service("Webapi/Soap", svc_class,
                                            svc_method TSRMLS_CC);

  nr_php_arg_release(&svc_class);
  nr_php_arg_release(&svc_method);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * Introduced in Magento 2.3.2
 * Convert arguments received from SOAP server to arguments to pass to a
 * service. Takes the following arguments:
 * string $serviceClass
 * array $methodMetadata
 * array $arguments
 *
 * *  txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This combination entails that the last wrapped function
 * call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_magento2_soap_handler_prepareoperationinput) {
  zval* svc_class = NULL;
  zval* svc_method = NULL;
  zval* method_metadata = NULL;

  (void)wraprec;
  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_MAGENTO2);

  svc_class = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  method_metadata = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  /*
   * We expect method_metadata to be an array.  At index 'method', if we see
   * a method name, we'll pass it to the transaction naming.
   * See:
   * https://www.magentoextensions.org/documentation/class_magento_1_1_webapi_1_1_model_1_1_service_metadata.html
   */

  if (!nr_php_is_zval_valid_array(method_metadata)) {
    nrl_verbosedebug(NRL_TXN, "Magento: $methodMetadata was not an array");
    goto end;
  }
  svc_method = nr_php_zend_hash_find(Z_ARRVAL_P(method_metadata), "method");

  if (NULL != svc_method) {
    nr_magento2_name_transaction_from_service("Webapi/Soap", svc_class,
                                              svc_method TSRMLS_CC);
  } else {
    nrl_verbosedebug(NRL_TXN,
                     "Magento: unable to determine method name from metadata.");
  }

end:
  nr_php_arg_release(&svc_class);
  nr_php_arg_release(&method_metadata);
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_magento2_ui_controller_execute) {
  (void)wraprec;
  nrl_verbosedebug(
      NRL_FRAMEWORK,
      "%s: Disabling auto instrumentation for Magento's text/html JSON",
      __func__);

  NRTXN(options.autorum_enabled) = 0;

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

void nr_magento2_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent name routing errors.
   */
  nr_txn_set_path("Magento", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  /*
   * Action is an abstract class that all controllers inherit. Note that if
   * dispatch() is overridden and the original method is never invoked, this
   * hook will not fire.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Magento\\Framework\\App\\Action\\Action::dispatch"),
      nr_magento2_action_dispatch, NULL);
#else
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Framework\\App\\Action\\Action::dispatch"),
      nr_magento2_action_dispatch TSRMLS_CC);
#endif

  /*
   * Kernel is Magento's built-in cache processor.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Framework\\App\\PageCache\\Kernel::load"),
      nr_magento2_pagecache_kernel_load TSRMLS_CC);

  /*
   * Interceptors use the "object manager" (Magento 2's DI container) to replace
   * the service. As described above, we need to catch requests for the
   * FrontControllerInterface to see if it was replaced.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Framework\\ObjectManager\\ObjectManager::get"),
      nr_magento2_objectmanager_get TSRMLS_CC);

  /*
   * The REST controller within Magento's Webapi package implements its own
   * entirely separate routing. We'll access the current route as the input
   * params are resolved.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR(
          "Magento\\Webapi\\Controller\\Rest\\InputParamsResolver::resolve"),
      nr_magento2_inputparamsresolver_resolve, NULL);
#else
  nr_php_wrap_user_function(
      NR_PSTR(
          "Magento\\Webapi\\Controller\\Rest\\InputParamsResolver::resolve"),
      nr_magento2_inputparamsresolver_resolve TSRMLS_CC);
#endif
  /*
   * The SOAP controller also implements its own routing logic. There are
   * effectively three cases in Magento\Webapi\Controller\Soap::dispatch():
   * listing the available WSDL endpoints, handling a WSDL endpoint, and
   * handling a SOAP request. These wrappers instrument each in turn.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap::_isWsdlRequest"),
      nr_magento2_soap_iswsdlrequest TSRMLS_CC);

  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap::_isWsdlListRequest"),
      nr_magento2_soap_iswsdllistrequest TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap\\Request\\Handler::_"
              "prepareRequestData"),
      nr_magento2_soap_handler_preparerequestdata, NULL);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap\\Request\\Handler::"
              "prepareOperationInput"),
      nr_magento2_soap_handler_prepareoperationinput, NULL);

#else
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap\\Request\\Handler::_"
              "prepareRequestData"),
      nr_magento2_soap_handler_preparerequestdata TSRMLS_CC);

  /*
   * Version 2.3.2 changed the call path for the Soap Handler from
   * `_prepareRequestData` to `prepareOperationInput`
   */

  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Webapi\\Controller\\Soap\\Request\\Handler::"
              "prepareOperationInput"),
      nr_magento2_soap_handler_prepareoperationinput TSRMLS_CC);
#endif

  /*
   * The Magento_Ui render controllers will, if sent a json Accepts
   * header, render their responses as a raw JSON string.  However,
   * Magento does not change the header to text/html, which means our
   * autorum insertion still happens, which can cause inconsistencies
   * if the the JSON contains a <head...> string. So we need to disable
   * autorum manually for these requests.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Ui\\Controller\\Index\\Render::execute"),
      nr_magento2_ui_controller_execute TSRMLS_CC);

  nr_php_wrap_user_function(
      NR_PSTR("Magento\\Ui\\Controller\\Adminhtml\\Index\\Render::execute"),
      nr_magento2_ui_controller_execute TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "magento",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
