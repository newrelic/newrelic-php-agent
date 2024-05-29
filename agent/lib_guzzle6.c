/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Guzzle is a general purpose library for making HTTP requests. It supports
 * asynchronous, parallel requests using curl_multi_exec() while providing a
 * modern OO API for users.
 *
 * It is a required component in Drupal 8, and strongly recommended by other
 * frameworks, including Symfony 2 and 3.
 *
 * Our approach for Guzzle 6 is to register middleware on every client that
 * adds our headers to the request object, handles responses, and creates
 * metrics and trace nodes using the internal RequestHandler class declared
 * below.
 *
 * There is one issue with this approach, which is that the middleware is
 * called when the request is created, rather than when the request is sent. As
 * Guzzle 6 removed the event system that allowed us to know exactly when the
 * request was sent, we are unable to get the time of the request being sent
 * without instrumenting much more deeply into Guzzle's handlers. We consider
 * this to be an obscure enough edge case that we are not doing this work at
 * present.
 *
 * An example of code that would have this problem is:
 *
 * $client = new Client;
 * $promise = $client->getAsync('http://httpbin.org/delay/1');
 * sleep(1);
 * Promise\unwrap($promise);
 *
 * The external metric created here would be 2 seconds, instead of 1, as the
 * sleep(1) would be considered to be external time.
 *
 * Source : https://github.com/guzzle/guzzle
 * Docs   : https://guzzle.readthedocs.org/en/latest/
 */

#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "lib_guzzle_common.h"
#include "lib_guzzle6.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "nr_txn.h"
#include "nr_txn.h"
#include "php_psr7.h"
#include "util_hashmap.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#include "ext/standard/php_var.h"

#define PHP_PACKAGE_NAME "guzzlehttp/guzzle"

/*
 * Since Guzzle 6 requires PHP 5.5.0 or later, we just won't build the Guzzle 6
 * support on older versions and will instead provide simple stubs for the two
 * exported functions to avoid linking errors.
 */
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO

/* {{{ newrelic\Guzzle6\RequestHandler class definition and methods */

/*
 * True global for the RequestHandler class entry.
 */
zend_class_entry* nr_guzzle6_requesthandler_ce;

/*
 * Arginfo for the RequestHandler methods.
 */
ZEND_BEGIN_ARG_INFO_EX(nr_guzzle6_requesthandler_construct_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, request)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_guzzle6_requesthandler_onfulfilled_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, response)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_guzzle6_requesthandler_onrejected_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, e)
ZEND_END_ARG_INFO()

static zval* nr_guzzle6_requesthandler_get_request(zval* obj TSRMLS_DC) {
  zval* prop;

  prop = nr_php_get_zval_object_property(obj, "request" TSRMLS_CC);
  if (NULL == prop) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: no request property", __func__);
    return NULL;
  }
  if (!nr_php_psr7_is_request(prop TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: request is not a Request", __func__);
    return NULL;
  }

  return prop;
}

static void nr_guzzle6_requesthandler_handle_response(zval* handler,
                                                      zval* response
                                                          TSRMLS_DC) {
  nr_segment_t* segment = NULL;
  nr_segment_external_params_t external_params = {.library = "Guzzle 6"};
  zval* request;
  zval* method;
  zval* status = NULL;

  if (NR_FAILURE
      == nr_guzzle_obj_find_and_remove(handler, &segment TSRMLS_CC)) {
    return;
  }

  request = nr_guzzle6_requesthandler_get_request(handler TSRMLS_CC);
  if (NULL == request) {
    return;
  }

  external_params.uri = nr_php_psr7_request_uri(request TSRMLS_CC);
  if (NULL == external_params.uri) {
    return;
  }

  method = nr_php_call(request, "getMethod");

  if (nr_php_is_zval_valid_string(method)) {
    external_params.procedure
        = nr_strndup(Z_STRVAL_P(method), Z_STRLEN_P(method));
  }

  if (NULL != response && nr_php_psr7_is_response(response TSRMLS_CC)) {
    /*
    * Get the X-NewRelic-App-Data response header. If there isn't one, NULL is
    * returned, and everything still works just fine.
    */
    external_params.encoded_response_header
        = nr_php_psr7_message_get_header(response, X_NEWRELIC_APP_DATA TSRMLS_CC);

    if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
      nrl_verbosedebug(
          NRL_CAT, "CAT: outbound response: transport='Guzzle 6' %s=" NRP_FMT,
          X_NEWRELIC_APP_DATA, NRP_CAT(external_params.encoded_response_header));
    }

    status = nr_php_call(response, "getStatusCode");

    if (nr_php_is_zval_valid_integer(status)) {
      external_params.status = Z_LVAL_P(status);
    }
  }

  nr_segment_external_end(&segment, &external_params);

  nr_free(external_params.encoded_response_header);
  nr_free(external_params.uri);
  nr_free(external_params.procedure);
  nr_php_zval_free(&method);
  nr_php_zval_free(&status);
}

/*
 * The method implementations for the RequestHandler class.
 */

/*
 * Proto   : void RequestHandler::__construct(Psr\Http\Message\RequestInterface
 * $request)
 */
static PHP_NAMED_FUNCTION(nr_guzzle6_requesthandler_construct) {
  zval* request = NULL;
  zval* this_obj = NULL;

  /*
   * A bunch of parameters are unused, so we'll suppress the errors.
   */
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  if (FAILURE
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "o", &request)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: did not get request", __func__);
    return;
  }

  this_obj = NR_PHP_USER_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  zend_update_property(Z_OBJCE_P(this_obj), ZVAL_OR_ZEND_OBJECT(this_obj),
                       NR_PSTR("request"), request TSRMLS_CC);

  nr_guzzle_obj_add(this_obj, "Guzzle 6" TSRMLS_CC);
}

/*
 * Proto   : void RequestHandler::onFulfilled(Psr\Http\Message\ResponseInterface
 * $response)
 *
 * Purpose : Called when a Guzzle 6 request promise is fulfilled.
 *
 * Params  : 1. The response object.
 */
static PHP_NAMED_FUNCTION(nr_guzzle6_requesthandler_onfulfilled) {
  zval* response = NULL;
  zval* this_obj = NULL;

  /*
   * Ignore unused parameters.
   */
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  /*
   * The return value should be ignored anyway, but let's make sure of it.
   */
  ZVAL_NULL(return_value);

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  if (FAILURE
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "o", &response)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: did not get response", __func__);
    return;
  }

  this_obj = NR_PHP_USER_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  nr_guzzle6_requesthandler_handle_response(this_obj, response TSRMLS_CC);
}

/*
 * Proto   : void
 * RequestHandler::onRejected(GuzzleHttp\Exception\TransferException $e)
 *
 * Purpose : Called when a Guzzle 6 request promise failed.
 *
 * Params  : 1. The exception object.
 */
static PHP_NAMED_FUNCTION(nr_guzzle6_requesthandler_onrejected) {
  zval* exc = NULL;
  zval* response = NULL;
  zval* this_obj = NULL;

  /*
   * Ignore unused parameters.
   */
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  /*
   * The return value should be ignored anyway, but let's make sure of it.
   */
  ZVAL_NULL(return_value);

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }

  if (FAILURE
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "o", &exc)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: did not get exception", __func__);
    return;
  }

  this_obj = NR_PHP_USER_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  /*
   * See if this is an exception that we can get a response from. We're going
   * to look for BadResponseException because, although it inherits from
   * RequestException (which theoretically is what provides the response), in
   * practice we don't get a usable response from anything other than the
   * children of BadResponseException.
   *
   * For the record, BadResponseException is what gets thrown when the user has
   * asked for HTTP errors (4XX and 5XX response codes) to be turned into
   * exceptions instead of being returned normally. In other external handling,
   * we still turn those into external nodes, so we shall also do so here.
   */
  if (!nr_php_object_instanceof_class(
          exc, "GuzzleHttp\\Exception\\BadResponseException" TSRMLS_CC)) {
    nr_guzzle6_requesthandler_handle_response(this_obj, NULL);
    return;
  }

  response = nr_php_call(exc, "getResponse");
  if (NULL == response) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: error calling getResponse", __func__);
    return;
  }

  nr_guzzle6_requesthandler_handle_response(this_obj, response TSRMLS_CC);

  nr_php_zval_free(&response);
}

/*
 * The method array for the RequestHandler class.
 */
const zend_function_entry nr_guzzle6_requesthandler_functions[]
    = {ZEND_FENTRY(__construct,
                   nr_guzzle6_requesthandler_construct,
                   nr_guzzle6_requesthandler_construct_arginfo,
                   ZEND_ACC_PUBLIC)
           ZEND_FENTRY(onFulfilled,
                       nr_guzzle6_requesthandler_onfulfilled,
                       nr_guzzle6_requesthandler_onfulfilled_arginfo,
                       ZEND_ACC_PUBLIC)
               ZEND_FENTRY(onRejected,
                           nr_guzzle6_requesthandler_onrejected,
                           nr_guzzle6_requesthandler_onrejected_arginfo,
                           ZEND_ACC_PUBLIC) PHP_FE_END};

/* }}} */

#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
void nr_guzzle6_client_construct(NR_EXECUTE_PROTO) {
#else
NR_PHP_WRAPPER_START(nr_guzzle6_client_construct) {
#endif
  zval* config;
  zend_class_entry* guzzle_client_ce;
  zval* handler_stack;
  zval* middleware = NULL;
  zval* retval;
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS);

  char* version = nr_php_get_object_constant(this_var, "VERSION");
  if (NULL == version) {
    version = nr_php_get_object_constant(this_var, "MAJOR_VERSION");
  }

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    // Add php package to transaction
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }
  nr_fw_support_add_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                                  version);
  nr_free(version);

#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
  (void)wraprec;
#endif
  NR_UNUSED_SPECIALFN;

  /* This is how we distinguish Guzzle 4/5. */
  if (nr_guzzle_does_zval_implement_has_emitter(this_var TSRMLS_CC)) {
#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
    NR_PHP_WRAPPER_CALL;
#endif
    goto end;
  }

#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
  NR_PHP_WRAPPER_CALL;
#endif

  /*
   * Get our middleware callable (which is just a string), and make sure it's
   * actually callable before we invoke push(). (See also PHP-1184.)
   */
  middleware = nr_php_zval_alloc();
  nr_php_zval_str(middleware, "newrelic\\Guzzle6\\middleware");
  if (!nr_php_is_zval_valid_callable(middleware TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: middleware string is not considered callable",
                     __func__);

    nrm_force_add(NRTXN(unscoped_metrics),
                  "Supportability/library/Guzzle 6/MiddlewareNotCallable", 0);

    goto end;
  }

  guzzle_client_ce = nr_php_find_class("guzzlehttp\\client" TSRMLS_CC);
  if (NULL == guzzle_client_ce) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: unable to get class entry for GuzzleHttp\\Client",
                     __func__);
    goto end;
  }

  config = nr_php_get_zval_object_property_with_class(
      this_var, guzzle_client_ce, "config" TSRMLS_CC);
  if (!nr_php_is_zval_valid_array(config)) {
    goto end;
  }

  handler_stack = nr_php_zend_hash_find(Z_ARRVAL_P(config), "handler");
  if (!nr_php_object_instanceof_class(handler_stack,
                                      "GuzzleHttp\\HandlerStack" TSRMLS_CC)) {
    goto end;
  }

  retval = nr_php_call(handler_stack, "push", middleware);

  nr_php_zval_free(&retval);

end:
  nr_php_zval_free(&middleware);
  nr_php_scope_release(&this_var);
}
#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
NR_PHP_WRAPPER_END
#endif

void nr_guzzle6_enable(TSRMLS_D) {
  int retval;

  if (0 == NRINI(guzzle_enabled)) {
    return;
  }

  /*
   * Here's something new: we're going to evaluate PHP code to build our
   * middleware in PHP, rather than doing it in C. This is mostly because it's
   * fairly difficult to return a higher-order function from C; while possible,
   * the code to do so is horrible enough that this actually feels cleaner.
   *
   * We'll do it when the library is detected because that should only happen
   * once, but we'll also be careful to put guards around the function
   * declaration just in case.
   *
   * On the bright side, zend_eval_string() effectively treats the string given
   * as a standalone file, so we can use a normal namespace declaration to
   * avoid possible clashes.
   */
  retval = zend_eval_string(
      "namespace newrelic\\Guzzle6;"

      "use Psr\\Http\\Message\\RequestInterface;"
      "use GuzzleHttp\\Promise\\PromiseInterface;"

      "if (!function_exists('newrelic\\Guzzle6\\middleware')) {"
      "  function middleware(callable $handler) {"
      "    return function (RequestInterface $request, array $options) use "
      "($handler) {"

      /*
       * Start by adding the outbound CAT/DT/Synthetics headers to the request.
       */
      "      foreach (newrelic_get_request_metadata('Guzzle 6') as $k => $v) {"
      "        $request = $request->withHeader($k, $v);"
      "      }"

      /*
       * Set up the RequestHandler object and attach it to the promise so that
       * we create an external node and deal with the CAT headers coming back
       * from the far end.
       */
      "      $rh = new RequestHandler($request);"
      "      $promise = $handler($request, $options);"
      "      if (PromiseInterface::REJECTED == $promise->getState()) {"
      /*
                Special case for sync request. When sync requests is rejected,
                onRejected callback is not called via `PromiseInterface::then`
                and needs to be called manually.
       */
      "        $rh->onRejected($promise);"
      "      } else {"
      "        $promise->then([$rh, 'onFulfilled'], [$rh, 'onRejected']);"
      "      }"
      "      return $promise;"
      "    };"
      "  }"
      "}",
      NULL, "newrelic/Guzzle6" TSRMLS_CC);

  if (SUCCESS == retval) {
    nr_php_wrap_user_function(NR_PSTR("GuzzleHttp\\Client::__construct"),
                              nr_guzzle_client_construct TSRMLS_CC);
  } else {
    nrl_warning(NRL_FRAMEWORK,
                "%s: error evaluating PHP code; not installing handler",
                __func__);
  }
}

void nr_guzzle6_minit(TSRMLS_D) {
  zend_class_entry ce;

  if (0 == NRINI(guzzle_enabled)) {
    return;
  }

  INIT_CLASS_ENTRY(ce, "newrelic\\Guzzle6\\RequestHandler",
                   nr_guzzle6_requesthandler_functions);
  nr_guzzle6_requesthandler_ce
      = nr_php_zend_register_internal_class_ex(&ce, NULL TSRMLS_CC);

  zend_declare_property_null(nr_guzzle6_requesthandler_ce, NR_PSTR("request"),
                             ZEND_ACC_PRIVATE TSRMLS_CC);
}

#else /* PHP < 5.5 */

NR_PHP_WRAPPER_START(nr_guzzle6_client_construct) {
  (void)wraprec;
  NR_UNUSED_SPECIALFN;
  NR_UNUSED_TSRMLS;
}
NR_PHP_WRAPPER_END

void nr_guzzle6_enable(TSRMLS_D) {
  NR_UNUSED_TSRMLS
}

void nr_guzzle6_minit(TSRMLS_D) {
  NR_UNUSED_TSRMLS;
}

#endif /* 5.5.x */
