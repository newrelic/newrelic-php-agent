/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Guzzle is a general purpose library for making HTTP requests. It supports
 * asynchronous, parallel requests using curl_multi_exec() while providing a
 * modern OO API for users.
 *
 * It is a required component in Drupal 8, and strongly recommended by other
 * frameworks, including Symfony 2.
 *
 * Our approach for Guzzle 4 and 5 is to use Guzzle's own event system: when a
 * GuzzleHttp\Client object is created, we attach a subscriber object that
 * registers its interest in the "before" and "complete" events (which are
 * basically what they sound like) and then tracks requests from there.
 *
 * Source : https://github.com/guzzle/guzzle
 * Docs   : https://guzzle.readthedocs.org/en/latest/
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "lib_guzzle_common.h"
#include "lib_guzzle4.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "util_logging.h"
#include "util_memory.h"

/*
 * We rely on the const correctness of certain Zend functions that weren't
 * const correct before 5.3 and/or 5.4: since Guzzle 4 requires 5.4.0 anyway,
 * we just won't build the Guzzle 4 support on older versions and will instead
 * provide simple stubs for the two exported functions to avoid linking errors.
 */
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO

/* {{{ Convenience functions for Guzzle interface checks */

/*
 * Purpose : Checks if the given object implements
 *           GuzzleHttp\Event\EventInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object does implement the interface; zero
 *           otherwise.
 */
static int nr_guzzle4_is_zval_an_event(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      obj, "GuzzleHttp\\Event\\EventInterface" TSRMLS_CC);
}

/*
 * Purpose : Checks if the given object implements
 *           GuzzleHttp\Event\EmitterInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object does implement the interface; zero
 *           otherwise.
 */
static int nr_guzzle4_is_zval_an_emitter(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      obj, "GuzzleHttp\\Event\\EmitterInterface" TSRMLS_CC);
}

/*
 * Purpose : Checks if the given object implements
 *           GuzzleHttp\Message\RequestInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object does implement the interface; zero
 *           otherwise.
 */
static int nr_guzzle4_is_zval_a_request(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      obj, "GuzzleHttp\\Message\\RequestInterface" TSRMLS_CC);
}

/*
 * Purpose : Checks if the given object implements
 *           GuzzleHttp\Message\ResponseInterface.
 *
 * Params  : 1. The object to check.
 *
 * Returns : Non-zero if the object does implement the interface; zero
 *           otherwise.
 */
static int nr_guzzle4_is_zval_a_response(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      obj, "GuzzleHttp\\Message\\ResponseInterface" TSRMLS_CC);
}

/* }}} */
/* {{{ Static functions used by Subscriber methods */

/*
 * Purpose : Adds an event definition to an events array, formatted in the form
 *           that Guzzle 4 expects from an object implementing
 *           SubscriberInterface.
 *
 * Params  : 1. The events array to add the event to.
 *           2. The event name.
 *           3. The method name that should be called when the event fires.
 *           4. The priority of the event. Guzzle's default is 0, and that's
 *              probably always the right value for us.
 */
static void nr_guzzle4_add_event_to_events_array(zval* events,
                                                 const char* event,
                                                 const char* method,
                                                 long priority) {
  zval* definition = nr_php_zval_alloc();

  array_init(definition);
  nr_php_add_next_index_string(definition, method);
  add_next_index_long(definition, priority);

  nr_php_add_assoc_zval(events, event, definition);
  nr_php_zval_free(&definition);
}

/*
 * A structure representing the expected arguments received by a Guzzle event
 * handler.
 */
typedef struct _nr_guzzle4_subscriber_event_args_t {
  zval* event;              /* The event object. */
  char* name;               /* The event name. */
  nr_string_len_t name_len; /* The length of the event name. */
} nr_guzzle4_subscriber_event_args_t;

/*
 * Purpose : Parses the parameters to an event handler function and validates
 *           that they are the expected values.
 *
 * Params  : 1. A pointer to a structure that will have the event arguments
 *              filled into it.
 *           2. PHP's internal function parameters.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, then fields
 *           in args should not be accessed.
 */
static nr_status_t nr_guzzle4_subscriber_event_get_args(
    nr_guzzle4_subscriber_event_args_t* args,
    INTERNAL_FUNCTION_PARAMETERS) {
  /*
   * A bunch of parameters are unused, so we'll suppress the errors.
   */
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (NULL == args) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Guzzle 4-5: %s got NULL args", __func__);
    return NR_FAILURE;
  }

  if (SUCCESS
      != zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "os", &args->event,
                               &args->name, &args->name_len)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Guzzle 4-5: zpp failed in %s", __func__);
    return NR_FAILURE;
  }

  if (!nr_guzzle4_is_zval_an_event(args->event TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: event is not an EventInterface in %s",
                     __func__);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

/* }}} */
/* {{{ newrelic\Guzzle4\Subscriber class definition and methods */

/*
 * True global for the Subscriber class entry.
 */
zend_class_entry* nr_guzzle4_subscriber_ce;

/*
 * Arginfo for the Subscriber methods.
 */
ZEND_BEGIN_ARG_INFO_EX(nr_guzzle4_subscriber_get_events_arginfo, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_guzzle4_subscriber_on_before_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, event)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_guzzle4_subscriber_on_complete_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, event)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

/*
 * The method implementations for the Subscriber class.
 */

/*
 * Proto   : array Subscriber::getEvents()
 *
 * Purpose : Returns an array containing the events that we want to subscribe
 *           to.
 *
 * Returns : An array, formatted in the style described in the Guzzle docs at
 *           http://docs.guzzlephp.org/en/latest/events.html#event-subscribers.
 */
static PHP_NAMED_FUNCTION(nr_guzzle4_subscriber_get_events) {
  /*
   * Ignore unused parameters.
   */
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  /*
   * Effectively, we're returning:
   * [
   *   'before'   => ['onBefore',   0],
   *   'complete' => ['onComplete', 0],
   * ]
   */
  array_init(return_value);

  nr_guzzle4_add_event_to_events_array(return_value, "before", "onBefore", 0);
  nr_guzzle4_add_event_to_events_array(return_value, "complete", "onComplete",
                                       0);
}

/*
 * Proto   : boolean Subscriber::onBefore(BeforeEvent $event, $name)
 *
 * Purpose : Handles the "before" event emitted by Guzzle 4 when a request is
 *           about to be sent.
 *
 * Params  : 1. The BeforeEvent object containing the request.
 *           2. The name of the event (ignored).
 *
 * Returns : True on success; false otherwise. These values are ignored by
 *           Guzzle 4, but may be useful for testing.
 */
static PHP_NAMED_FUNCTION(nr_guzzle4_subscriber_on_before) {
  nr_guzzle4_subscriber_event_args_t args;
  zval* request = NULL;
  nr_segment_t* segment;

  if (NR_FAILURE
      == nr_guzzle4_subscriber_event_get_args(
          &args, INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: onBefore() received unexpected arguments");
    RETURN_FALSE;
  }

  /*
   * Pull the request out of the event object.
   */
  request = nr_php_call(args.event, "getRequest");
  if (!nr_guzzle4_is_zval_a_request(request TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: onBefore() event did not return a request");
    RETURN_FALSE;
  }

  /*
   * Add the request object to those we're tracking.
   */
  segment = nr_guzzle_obj_add(request, "Guzzle 4" TSRMLS_CC);

  /*
   * Set the request headers.
   */
  nr_guzzle_request_set_outbound_headers(request, segment TSRMLS_CC);

  nr_php_zval_free(&request);
  RETURN_TRUE;
}

/*
 * Proto   : boolean Subscriber::onComplete(BeforeEvent $event, $name)
 *
 * Purpose : Handles the "complete" event emitted by Guzzle 4 when a request is
 *           about to be sent.
 *
 * Params  : 1. The CompleteEvent object containing the request and response.
 *           2. The name of the event (ignored).
 *
 * Returns : True on success; false otherwise. These values are ignored by
 *           Guzzle 4, but may be useful for testing.
 */
static PHP_NAMED_FUNCTION(nr_guzzle4_subscriber_on_complete) {
  nr_guzzle4_subscriber_event_args_t args;
  zval* request = NULL;
  zval* response = NULL;
  zval* status = NULL;
  zval* method = NULL;
  nr_segment_t* segment;
  nr_segment_external_params_t external_params = {.library = "Guzzle 4/5"};
  zval* url = NULL;

  if (NR_FAILURE
      == nr_guzzle4_subscriber_event_get_args(
          &args, INTERNAL_FUNCTION_PARAM_PASSTHRU)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: onComplete() received unexpected arguments");
    RETVAL_FALSE;
    goto leave;
  }

  /*
   * Pull the request and response out of the event object.
   */
  request = nr_php_call(args.event, "getRequest");
  if (!nr_guzzle4_is_zval_a_request(request TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: onComplete() event did not return a request");
    RETVAL_FALSE;
    goto leave;
  }

  response = nr_php_call(args.event, "getResponse");
  if (!nr_guzzle4_is_zval_a_response(response TSRMLS_CC)) {
    nrl_verbosedebug(
        NRL_FRAMEWORK,
        "Guzzle 4-5: onComplete() event did not return a response");
    RETVAL_FALSE;
    goto leave;
  }

  /*
   * Find the original start time for the request.
   */
  if (NR_FAILURE
      == nr_guzzle_obj_find_and_remove(request, &segment TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle 4-5: Request completed without being tracked");
    RETVAL_FALSE;
    goto leave;
  }

  /*
   * We also need the URL to create a useful metric.
   */
  url = nr_php_call(request, "getUrl");
  if (!nr_php_is_zval_valid_string(url)) {
    RETVAL_FALSE;
    goto leave;
  }
  external_params.uri = nr_strndup(Z_STRVAL_P(url), Z_STRLEN_P(url));

  status = nr_php_call(response, "getStatusCode");

  if (nr_php_is_zval_valid_integer(status)) {
    external_params.status = Z_LVAL_P(status);
  }

  /*
   * Grab the X-NewRelic-App-Data response header, if there is one. We don't
   * check for a valid string below as it's not an error if the header doesn't
   * exist (and hence NULL is returned).
   */
  external_params.encoded_response_header
      = nr_guzzle_response_get_header(X_NEWRELIC_APP_DATA, response TSRMLS_CC);

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT, "CAT: outbound response: transport='Guzzle 4-5' %s=" NRP_FMT,
        X_NEWRELIC_APP_DATA, NRP_CAT(external_params.encoded_response_header));
  }

  method = nr_php_call(request, "getMethod");

  if (nr_php_is_zval_valid_string(method)) {
    external_params.procedure
        = nr_strndup(Z_STRVAL_P(method), Z_STRLEN_P(method));
  }

  /*
   * Unlike Guzzle 3, we don't have any metadata available from Guzzle itself
   * to answer the question of how long the request took. Instead, we'll assume
   * that curl_multi_exec() calls back reasonably efficiently and just take the
   * wallclock time up to now.
   */
  nr_segment_external_end(&segment, &external_params);

  RETVAL_TRUE;

leave:
  nr_free(external_params.uri);
  nr_free(external_params.encoded_response_header);
  nr_free(external_params.procedure);
  nr_php_zval_free(&method);
  nr_php_zval_free(&request);
  nr_php_zval_free(&response);
  nr_php_zval_free(&url);
  nr_php_zval_free(&status);
}

/*
 * The method array for the Subscriber class.
 */
const zend_function_entry nr_guzzle4_subscriber_functions[]
    = {ZEND_FENTRY(getEvents,
                   nr_guzzle4_subscriber_get_events,
                   nr_guzzle4_subscriber_get_events_arginfo,
                   ZEND_ACC_PUBLIC)
           ZEND_FENTRY(onBefore,
                       nr_guzzle4_subscriber_on_before,
                       nr_guzzle4_subscriber_on_before_arginfo,
                       ZEND_ACC_PUBLIC)
               ZEND_FENTRY(onComplete,
                           nr_guzzle4_subscriber_on_complete,
                           nr_guzzle4_subscriber_on_complete_arginfo,
                           ZEND_ACC_PUBLIC) PHP_FE_END};

/* }}} */

/*
 * Purpose : Registers an event subscriber for a newly instantiated
 *           GuzzleHttp\Client object.
 */

#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
void nr_guzzle4_client_construct(NR_EXECUTE_PROTO) {
#else
NR_PHP_WRAPPER_START(nr_guzzle4_client_construct) {
#endif
  zval* emitter = NULL;
  zval* retval = NULL;
  zval* subscriber = NULL;
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
  (void)wraprec;
#endif
  NR_UNUSED_SPECIALFN;

  /* This is how we distinguish Guzzle 4/5 from other versions. */
  if (0 == nr_guzzle_does_zval_implement_has_emitter(this_var TSRMLS_CC)) {
#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
    NR_PHP_WRAPPER_CALL;
#endif
    goto end;
  }

#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
  NR_PHP_WRAPPER_CALL;
#endif

  /*
   * We can't have newrelic\Guzzle4\Subscriber implement
   * GuzzleHttp\Event\SubscriberInterface when the class is registered on
   * MINIT, because SubscriberInterface doesn't exist at that point. Instead,
   * we'll check now if the inheritance relationship has been set up, and if it
   * hasn't, then we'll set that up via zend_class_implements().
   */
  if (0
      == nr_php_class_entry_instanceof_class(
          nr_guzzle4_subscriber_ce,
          "GuzzleHttp\\Event\\SubscriberInterface" TSRMLS_CC)) {
    zend_class_entry* subscriber_interface
        = nr_php_find_class("guzzlehttp\\event\\subscriberinterface" TSRMLS_CC);

    if (subscriber_interface) {
      zend_class_implements(nr_guzzle4_subscriber_ce TSRMLS_CC, 1,
                            subscriber_interface);
    } else {
      nrl_info(NRL_FRAMEWORK,
               "Guzzle 4-5: cannot find SubscriberInterface class entry");
      goto end;
    }
  }

  /* Register the subscriber. */
  emitter = nr_php_call(this_var, "getEmitter");
  if (!nr_guzzle4_is_zval_an_emitter(emitter TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Guzzle 4-5: Client::getEmitter() didn't return an "
                     "EmitterInterface object");
    goto end;
  }

  subscriber = nr_php_zval_alloc();
  object_init_ex(subscriber, nr_guzzle4_subscriber_ce);

  retval = nr_php_call(emitter, "attach", subscriber);
  if (NULL == retval) {
    nrl_info(NRL_FRAMEWORK, "Guzzle 4-5: Emitter::attach() failed");
    nr_php_zval_free(&subscriber);
    goto end;
  }
  nr_php_zval_free(&retval);
  nrl_verbosedebug(NRL_FRAMEWORK, "Guzzle 4-5: subscriber attached to emitter");

end:
  nr_php_scope_release(&this_var);
  nr_php_zval_free(&emitter);
  nr_php_zval_free(&subscriber);
}
#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
NR_PHP_WRAPPER_END
#endif

void nr_guzzle4_enable(TSRMLS_D) {
  if (0 == NRINI(guzzle_enabled)) {
    return;
  }

  /*
   * Instrument Client::__construct() so we can register an event subscriber
   * when clients are instantiated. Guzzle 4 documents that you can attach
   * event handlers to Client objects and that you will then receive events
   * for all requests created on that client.
   */
  nr_php_wrap_user_function(NR_PSTR("GuzzleHttp\\Client::__construct"),
                            nr_guzzle_client_construct TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "guzzlehttp/guzzle",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}

void nr_guzzle4_minit(TSRMLS_D) {
  zend_class_entry ce;

  if (0 == NRINI(guzzle_enabled)) {
    return;
  }

  /*
   * Initialise the Guzzle 4 and 5 event subscriber class.
   */
  INIT_CLASS_ENTRY(ce, "newrelic\\Guzzle4\\Subscriber",
                   nr_guzzle4_subscriber_functions);
  nr_guzzle4_subscriber_ce
      = nr_php_zend_register_internal_class_ex(&ce, NULL TSRMLS_CC);

  /* Don't handle the implementation of the interface here, since we have to
   * do that during a request. */
}

void nr_guzzle4_rshutdown(TSRMLS_D) {
  zend_class_entry* iface_ce = NULL;

  if (0 == NRINI(guzzle_enabled)) {
    return;
  }

  /*
   * We need to uninherit Subscriber from SubscriberInterface, otherwise we
   * may cause crashes by pointing to a destroyed class entry.
   *
   * Of course, if SubscriberInterface was never declared, we're good. Note
   * that nr_php_find_class requires the lowercase version of the class name.
   */
  iface_ce
      = nr_php_find_class("guzzlehttp\\event\\subscriberinterface" TSRMLS_CC);
  if (NULL == iface_ce) {
    return;
  }

  nr_php_remove_interface_from_class(nr_guzzle4_subscriber_ce,
                                     iface_ce TSRMLS_CC);
}

#else /* PHP >= 5.4.0 */

/*
 * Stub implementations of the exported functions from this module for
 * PHP < 5.4.
 */

NR_PHP_WRAPPER_START(nr_guzzle4_client_construct) {
  (void)wraprec;
  NR_UNUSED_SPECIALFN;
  NR_UNUSED_TSRMLS;
}
NR_PHP_WRAPPER_END

void nr_guzzle4_enable(TSRMLS_D) {
  NR_UNUSED_TSRMLS
}

void nr_guzzle4_minit(TSRMLS_D) {
  NR_UNUSED_TSRMLS
}

void nr_guzzle4_rshutdown(TSRMLS_D) {
  NR_UNUSED_TSRMLS
}

#endif /* PHP >= 5.4.0 */
