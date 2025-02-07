/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the php-ampqlib
 * https://github.com/php-amqplib/php-amqplib
 */
#include "php_agent.h"
#include "php_api_distributed_trace.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "lib_php_amqplib.h"
#include "nr_segment_message.h"
#include "nr_header.h"

#define PHP_PACKAGE_NAME "php-amqplib/php-amqplib"

/*
 * With PHP 8+, we have access to all the zend_execute_data structures both
 * before and after the function call so we can just maintain pointers into the
 * struct.  With PHP 7.x, without doing special handling, we don't have access
 * to the values afterwards.  Sometimes nr_php_arg_get is used as that DUPs the
 * zval which then later needs to be freed with nr_php_arg_release.  In this
 * case, we don't need to go through the extra trouble of duplicating a ZVAL
 * when we don't need to duplicate anything if there is no valid value.  We
 * check for a valid value, and if we want to store it, we'll strdup it. So
 * instead of doing multiple zval dups all of the time, we do some strdups some
 * of the time.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8.0+ */
#define ENSURE_PERSISTENCE(x) x
#define UNDO_PERSISTENCE(x)
#else
#define ENSURE_PERSISTENCE(x) nr_strdup(x)
#define UNDO_PERSISTENCE(x) nr_free(x);
#endif

/*
 * See here for supported Amazon MQ for RabbitMQ engine versions
 * https://docs.aws.amazon.com/amazon-mq/latest/developer-guide/rabbitmq-version-management.html
 * For instance:
 * As of Feb 2025, 3.13 (recommended)
 *
 * See here for latest RabbitMQ Server https://www.rabbitmq.com/docs/download
 * For instance:
 * As of Feb 2025, the latest release of RabbitMQ Server is 4.0.5.
 *
 * https://www.rabbitmq.com/tutorials/tutorial-one-php
 * Installing RabbitMQ
 *
 * While the RabbitMQ tutorial for using with the dockerized RabbitMQ setup
 * correctly and loads the PhpAmqpLib\\Channel\\AMQPChannel class in time for
 * the agent to wrap the instrumented functions, with AWS MQ_BROKER
 * specific but valid scenarios where the PhpAmqpLib\\Channel\\AMQPChannel class
 * file does not explicitly load and the instrumented
 * functions are NEVER wrapped regardless of how many times they are called in
 * one txn.
 * Specifically, this centered around the very slight but impactful
 * differences when using managing the AWS MQ_BROKER connect vs using the
 * official RabbitMq Server, and this function is needed ONLY to support AWS's
 * MQ_BROKER.
 *
 * When connecting via SSL with rabbitmq's official server is explicitly loaded.
 * Hoever, when connecting via SSL with an MQ_BROKER that uses RabbitMQ(using
 * the exact same php file and with only changes in the server name for the
 * connection), the AMQPChannel file (and therefore class), the AMQPChannel file
 * (and therefore class) is NOT explicitly loaded.
 *
 * Because the very key `PhpAmqpLib/Channel/AMQPChannel.php` file never gets
 * explicitly loaded when interacting with the AWS MQ_BROKER, the class is not
 * automatically loaded even though it is available and can be resolved if
 * called from within PHP.  Because of this, the instrumented functions NEVER
 * get wrapped when connecting to the MQ_BROKER and therefore the
 * instrumentation is never triggered.  The explicit loading of the class is
 * needed to work with MQ_BROKER.
 */

/*
 * Purpose : Ensures the php-amqplib instrumentation gets wrapped.
 *
 * Params  : None
 *
 * Returns : None
 */
static void nr_php_amqplib_ensure_class() {
  int result = FAILURE;

  result = zend_eval_string("class_exists('PhpAmqpLib\\Channel\\AMQPChannel');",
                            NULL, "Get nr_php_amqplib_class_exists");
  /*
   * We don't need to check anything else at this point. If this fails, there's
   * nothing else we can do anyway.
   */
}

/*
 * Version detection will be called pulled from PhpAmqpLib\\Package::VERSION
 * nr_php_amqplib_handle_version will automatically load the class if it isn't
 * loaded yet and then evaluate the string. To avoid the VERY unlikely but not
 * impossible fatal error if the file/class doesn't exist, we need to wrap
 * the call in a try/catch block and make it a lambda so that we avoid errors.
 * This won't load the file if it doesn't exist, but by the time this is called,
 * the existence of the php-amqplib is a known quantity so calling the following
 * lambda will result in the PhpAmqpLib\\Package class being loaded.
 */
void nr_php_amqplib_handle_version() {
  char* version = NULL;
  zval retval_zpd;
  int result = FAILURE;

  result = zend_eval_string(
      "(function() {"
      "     $nr_php_amqplib_version = '';"
      "     try {"
      "          $nr_php_amqplib_version = PhpAmqpLib\\Package::VERSION;"
      "     } catch (Throwable $e) {"
      "     }"
      "     return $nr_php_amqplib_version;"
      "})();",
      &retval_zpd, "Get nr_php_amqplib_version");

  /* See if we got a non-empty/non-null string for version. */
  if (SUCCESS == result) {
    if (nr_php_is_zval_non_empty_string(&retval_zpd)) {
      version = Z_STRVAL(retval_zpd);
    }
  }

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Add php package to transaction */
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }

  nr_txn_suggest_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                               version);

  zval_dtor(&retval_zpd);
}

/*
 * Purpose : Retrieves host and port from an AMQP Connection and sets the
 * host/port values in the message_params.
 *
 * Params  : 1. PhpAmqpLib\Connection family of connections that inherit from
 * AbstractConnection
 *           2. nr_segment_message_params_t* message_params that will be
 * modified with port and host info, if available
 *
 * Returns : None
 *
 * See here for more information about the AbstractConnection class that all
 * Connection classes inherit from:
 * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Connection/AbstractConnection.php
 */
static inline void nr_php_amqplib_get_host_and_port(
    zval* amqp_connection,
    nr_segment_message_params_t* message_params) {
  zval* amqp_server = NULL;
  zval* amqp_port = NULL;
  zval* connect_constructor_params = NULL;

  if (NULL == amqp_connection || NULL == message_params) {
    return;
  }

  if (!nr_php_is_zval_valid_object(amqp_connection)) {
    return;
  }

  /* construct_params are always saved to use for cloning purposes. */
  connect_constructor_params
      = nr_php_get_zval_object_property(amqp_connection, "construct_params");
  if (!nr_php_is_zval_valid_array(connect_constructor_params)) {
    return;
  }

  amqp_server
      = nr_php_zend_hash_index_find(Z_ARRVAL_P(connect_constructor_params),
                                    AMQP_CONSTRUCT_PARAMS_SERVER_INDEX);
  if (nr_php_is_zval_non_empty_string(amqp_server)) {
    message_params->server_address
        = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_server));
  }

  amqp_port = nr_php_zend_hash_index_find(
      Z_ARRVAL_P(connect_constructor_params), AMQP_CONSTRUCT_PARAMS_PORT_INDEX);
  if (nr_php_is_zval_valid_integer(amqp_port)) {
    message_params->server_port = Z_LVAL_P(amqp_port);
  }
}

/*
 * Purpose : Applies DT headers to an outbound AMQPMessage.
 * Note:
 * The DT header 'newrelic' will only be added if both
 * newrelic.distributed_tracing_enabled is enabled and
 * newrelic.distributed_tracing_exclude_newrelic_header is set to false in the
 * INI settings. The W3C headers 'traceparent' and 'tracestate' will will only
 * be added if newrelic.distributed_tracing_enabled is enabled in the
 * newrelic.ini settings.
 *
 * Params  : PhpAmqpLib\Message\AMQPMessage
 *
 * Returns : None
 *
 * Refer here for AMQPMessage:
 * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Message/AMQPMessage.php
 * Refer here for AMQPTable:
 * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Wire/AMQPTable.php
 */

static inline void nr_php_amqplib_insert_dt_headers(zval* amqp_msg) {
  zval* amqp_properties_array = NULL;
  zval* dt_headers_zvf = NULL;
  zval* amqp_headers_table = NULL;
  zval* retval_set_property_zvf = NULL;
  zval* retval_set_table_zvf = NULL;
  zval application_headers_zpd;
  zval key_zval_zpd;
  zval amqp_table_retval_zpd;
  zval* key_exists = NULL;
  zval* amqp_table_data = NULL;
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;
  int retval = FAILURE;

  /*
   * Note:
   * The DT header 'newrelic' will only be added if both
   * newrelic.distributed_tracing_enabled is enabled and
   * newrelic.distributed_tracing_exclude_newrelic_header is set to false in the
   * INI settings. The W3C headers 'traceparent' and 'tracestate' will will only
   * be added if newrelic.distributed_tracing_enabled is enabled in the
   * newrelic.ini settings.
   */

  /*
   * Refer here for AMQPMessage:
   * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Message/AMQPMessage.php
   * Refer here for AMQPTable:
   * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Wire/AMQPTable.php
   */
  if (!nr_php_is_zval_valid_object(amqp_msg)) {
    return;
  }

  if (!NRPRG(txn)->options.distributed_tracing_enabled) {
    return;
  }

  amqp_properties_array
      = nr_php_get_zval_object_property(amqp_msg, "properties");
  if (!nr_php_is_zval_valid_array(amqp_properties_array)) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "AMQPMessage properties are invalid. AMQPMessage always sets "
        "this to empty arrray by default so something is seriously wrong with "
        "the message object. Exit.");
    return;
  }

  /*
   * newrelic_get_request_metadata is an internal API that will only return the
   * DT header 'newrelic' will only be added if both
   * newrelic.distributed_tracing_enabled is enabled and
   * newrelic.distributed_tracing_exclude_newrelic_header is set to false in the
   * INI settings. The W3C headers 'traceparent' and 'tracestate' will will only
   * be returned if newrelic.distributed_tracing_enabled is enabled in the
   * newrelic.ini settings.
   */
  dt_headers_zvf = nr_php_call(NULL, "newrelic_get_request_metadata");
  if (!nr_php_is_zval_valid_array(dt_headers_zvf)) {
    nr_php_zval_free(&dt_headers_zvf);
    return;
  }

  /*
   * The application_headers are stored in an encoded PhpAmqpLib\Wire\AMQPTable
   * object
   */

  amqp_headers_table = nr_php_zend_hash_find(Z_ARRVAL_P(amqp_properties_array),
                                             "application_headers");
  /*
   * If the application_headers AMQPTable object doesn't exist, we'll have to
   * create it with an empty array.
   */
  if (!nr_php_is_zval_valid_object(amqp_headers_table)) {
    retval = zend_eval_string(
        "(function() {"
        "     try {"
        "          return new PhpAmqpLib\\Wire\\AMQPTable(array());"
        "     } catch (Throwable $e) {"
        "          return null;"
        "     }"
        "})();",
        &amqp_table_retval_zpd, "newrelic.amqplib.add_empty_headers");

    if (FAILURE == retval) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "No application headers in AMQPTable, but couldn't "
                       "create one. Exit.");
      goto end;
    }
    if (!nr_php_is_zval_valid_object(&amqp_table_retval_zpd)) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "No application headers in AMQPTable, but couldn't "
                       "create one. Exit.");
      zval_ptr_dtor(&amqp_table_retval_zpd);
      goto end;
    }
    /*
     * Get application+_headers string in zval form for use with nr_php_call
     */
    ZVAL_STRING(&application_headers_zpd, "application_headers");
    /*
     * Set the valid AMQPTable on the AMQPMessage.
     */
    retval_set_property_zvf = nr_php_call(
        amqp_msg, "set", &application_headers_zpd, &amqp_table_retval_zpd);

    zval_ptr_dtor(&application_headers_zpd);
    zval_ptr_dtor(&amqp_table_retval_zpd);

    if (NULL == retval_set_property_zvf) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "AMQPMessage had no application_headers AMQPTable, but "
                       "set failed for the AMQPTable wthat was just created "
                       "for the application headers. Unable to proceed, exit.");
      goto end;
    }
    /* Should have valid AMQPTable objec on the AMQPMessage at this point. */
    amqp_headers_table = nr_php_zend_hash_find(
        Z_ARRVAL_P(amqp_properties_array), "application_headers");
    if (!nr_php_is_zval_valid_object(amqp_headers_table)) {
      nrl_info(
          NRL_INSTRUMENT,
          "AMQPMessage had no application_headers AMQPTable, but unable to "
          "retrieve even after creating and setting. Unable to proceed, exit.");
      goto end;
    }
  }

  /*
   * This contains the application_headers data. It is an array of
   * key/encoded_array_val pairs.
   */
  amqp_table_data = nr_php_get_zval_object_property(amqp_headers_table, "data");

  /*
   * First check if it's a reference to another zval, and if so, get point to
   * the actual zval.
   */

  if (IS_REFERENCE == Z_TYPE_P(amqp_table_data)) {
    amqp_table_data = Z_REFVAL_P(amqp_table_data);
  }
  if (!nr_php_is_zval_valid_array(amqp_table_data)) {
    /*
     * This is a basic part of the AMQPTable, if this doesn't exist, something
     * is seriously wrong.  Cannot proceed, exit.
     */
    goto end;
  }

  /*
   * Loop through the DT Header array and set the headers in the
   * application_header AMQPTable if they do not already exist.
   */

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(dt_headers_zvf), key_num, key_str, val) {
    (void)key_num;

    if (NULL != key_str && nr_php_is_zval_valid_string(val)) {
      key_exists
          = nr_php_zend_hash_find(HASH_OF(amqp_table_data), ZSTR_VAL(key_str));
      if (NULL == key_exists) {
        /* Key doesn't exist, so set the value in the AMQPTable. */

        /* key_str is a zend_string. It needs to be a zval to pass to
         * nr_php_call. */
        ZVAL_STR_COPY(&key_zval_zpd, key_str);
        retval_set_table_zvf
            = nr_php_call(amqp_headers_table, "set", &key_zval_zpd, val);
        if (NULL == retval_set_table_zvf) {
          nrl_verbosedebug(NRL_INSTRUMENT,
                           "%s didn't exist in the AMQPTable, but couldn't "
                           "set the key/val to the table.",
                           NRSAFESTR(ZSTR_VAL(key_str)));
        }
        zval_ptr_dtor(&key_zval_zpd);
        nr_php_zval_free(&retval_set_table_zvf);
      }
    }
  }
  ZEND_HASH_FOREACH_END();

end:
  nr_php_zval_free(&dt_headers_zvf);
  nr_php_zval_free(&retval_set_property_zvf);
}

/*
 * Purpose : Retrieve any DT headers from an inbound AMQPMessage if
 * newrelic.distributed_tracing_exclude_newrelic_header INI setting is false
 * and apply to txn.
 *
 * Params  : PhpAmqpLib\Message\AMQPMessage
 *
 * Returns : None
 *
 * Refer here for AMQPMessage:
 * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Message/AMQPMessage.php
 * Refer here for AMQPTable:
 * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Wire/AMQPTable.php
 */
static inline void nr_php_amqplib_retrieve_dt_headers(zval* amqp_msg) {
  zval* amqp_headers_native_data_zvf = NULL;
  zval* amqp_properties_array = NULL;
  zval* amqp_headers_table = NULL;
  zval* amqp_table_data = NULL;
  zval* dt_payload = NULL;
  zval* traceparent = NULL;
  zval* tracestate = NULL;
  char* dt_payload_string = NULL;
  char* traceparent_string = NULL;
  char* tracestate_string = NULL;
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;
  int retval = FAILURE;

  /*
   * Refer here for AMQPMessage:
   * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Message/AMQPMessage.php
   * Refer here for AMQPTable:
   * https://github.com/php-amqplib/php-amqplib/blob/master/PhpAmqpLib/Wire/AMQPTable.php
   */
  if (!nr_php_is_zval_valid_object(amqp_msg)) {
    return;
  }

  if (!NRPRG(txn)->options.distributed_tracing_enabled) {
    return;
  }

  amqp_properties_array
      = nr_php_get_zval_object_property(amqp_msg, "properties");
  if (!nr_php_is_zval_valid_array(amqp_properties_array)) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "AMQPMessage properties not valid. AMQPMessage always sets "
        "this to empty arrray by default. something seriously wrong with "
        "the message object. Unable to proceed, Exit");
    return;
  }

  /* PhpAmqpLib\Wire\AMQPTable object*/
  amqp_headers_table = nr_php_zend_hash_find(Z_ARRVAL_P(amqp_properties_array),
                                             "application_headers");
  if (!nr_php_is_zval_valid_object(amqp_headers_table)) {
    /* No headers here, exit. */
    return;
  }

  /*
   * We can't use amqp table "data" property here because while it has the
   * correct keys, the vals are encoded arrays. We need to use getNativeData
   * so it will decode the values for us since it formats the AMQPTable as an
   * array of unencoded key/val pairs. */
  amqp_headers_native_data_zvf
      = nr_php_call(amqp_headers_table, "getNativeData");

  if (!nr_php_is_zval_valid_array(amqp_headers_native_data_zvf)) {
    nr_php_zval_free(&amqp_headers_native_data_zvf);
    return;
  }

  dt_payload
      = nr_php_zend_hash_find(HASH_OF(amqp_headers_native_data_zvf), NEWRELIC);
  dt_payload_string
      = nr_php_is_zval_valid_string(dt_payload) ? Z_STRVAL_P(dt_payload) : NULL;

  traceparent = nr_php_zend_hash_find(HASH_OF(amqp_headers_native_data_zvf),
                                      W3C_TRACEPARENT);
  traceparent_string = nr_php_is_zval_valid_string(traceparent)
                           ? Z_STRVAL_P(traceparent)
                           : NULL;

  tracestate = nr_php_zend_hash_find(HASH_OF(amqp_headers_native_data_zvf),
                                     W3C_TRACESTATE);
  tracestate_string
      = nr_php_is_zval_valid_string(tracestate) ? Z_STRVAL_P(tracestate) : NULL;

  if (NULL != dt_payload || NULL != traceparent) {
    nr_hashmap_t* header_map = nr_header_create_distributed_trace_map(
        dt_payload_string, traceparent_string, tracestate_string);

    /*
     * nr_php_api_accept_distributed_trace_payload_httpsafe will add the headers
     * to the txn if there have been no other inbound/outbound headers added
     * already.
     */
    nr_php_api_accept_distributed_trace_payload_httpsafe(NRPRG(txn), header_map,
                                                         "Queue");

    nr_hashmap_destroy(&header_map);
  }
  nr_php_zval_free(&amqp_headers_native_data_zvf);

  return;
}

/*
 * Purpose : A wrapper to instrument the php-amqplib basic_publish.  This
 * retrieves values to populate a message segment and insert the DT headers, if
 * applicable.
 *
 * Note: The DT header 'newrelic' will only be added if both
 * newrelic.distributed_tracing_enabled is enabled and
 * newrelic.distributed_tracing_exclude_newrelic_header is set to false in the
 * INI settings. The W3C headers 'traceparent' and 'tracestate' will will only
 * be added if newrelic.distributed_tracing_enabled is enabled in the
 * newrelic.ini settings.
 *
 * PhpAmqpLib\Channel\AMQPChannel::basic_publish
 * Publishes a message
 *
 * @param AMQPMessage $msg
 * @param string $exchange
 * @param string $routing_key
 * @param bool $mandatory
 * @param bool $immediate
 * @param int|null $ticket
 * @throws AMQPChannelClosedException
 * @throws AMQPConnectionClosedException
 * @throws AMQPConnectionBlockedException
 *
 */

NR_PHP_WRAPPER(nr_rabbitmq_basic_publish_before) {
  zval* amqp_msg = NULL;
  (void)wraprec;

  amqp_msg = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS);
  /*
   * nr_php_amqplib_insert_dt_headers will check the validity of the object.
   */
  nr_php_amqplib_insert_dt_headers(amqp_msg);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_rabbitmq_basic_publish) {
  zval* amqp_exchange = NULL;
  zval* amqp_routing_key = NULL;
  zval* amqp_connection = NULL;
  nr_segment_t* message_segment = NULL;

  nr_segment_message_params_t message_params = {
      .library = RABBITMQ_LIBRARY_NAME,
      .destination_type = NR_MESSAGE_DESTINATION_TYPE_EXCHANGE,
      .message_action = NR_SPANKIND_PRODUCER,
      .messaging_system = RABBITMQ_MESSAGING_SYSTEM,
  };

  (void)wraprec;

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO /* PHP8.0+ */
  zval* amqp_msg = NULL;
  amqp_msg = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS);
  /*
   * nr_php_amqplib_insert_dt_headers will check the validity of the object.
   */
  nr_php_amqplib_insert_dt_headers(amqp_msg);
#endif

  amqp_exchange = nr_php_get_user_func_arg(2, NR_EXECUTE_ORIG_ARGS);
  if (nr_php_is_zval_non_empty_string(amqp_exchange)) {
    /*
     * In PHP 7.x, the following will create a strdup in
     * message_params.destination_name that needs to be freed.
     */
    message_params.destination_name
        = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_exchange));
  } else {
    /*
     * For producer, this is exchange name.  Exchange name is Default in case
     * of empty string.
     */
    if (nr_php_is_zval_valid_string(amqp_exchange)) {
      message_params.destination_name = ENSURE_PERSISTENCE("Default");
    }
  }

  amqp_routing_key = nr_php_get_user_func_arg(3, NR_EXECUTE_ORIG_ARGS);
  if (nr_php_is_zval_non_empty_string(amqp_routing_key)) {
    /*
     * In PHP 7.x, the following will create a strdup in
     * message_params.messaging_destination_routing_key that needs to be
     * freed.
     */
    message_params.messaging_destination_routing_key
        = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_routing_key));
  }

  amqp_connection = nr_php_get_zval_object_property(
      nr_php_execute_scope(execute_data), "connection");
  /*
   * In PHP 7.x, the following will create a strdup in
   * message_params.server_address that needs to be freed.
   */
  nr_php_amqplib_get_host_and_port(amqp_connection, &message_params);

  /* For PHP 7.x compatibility. */
  NR_PHP_WRAPPER_CALL

  /*
   * Now create and end the instrumented segment as a message segment.
   *
   * By this point, it's been determined that this call will be instrumented
   * so only create the message segment now, grab the parent segment start
   * time, add our message segment attributes/metrics then close the newly
   * created message segment.
   */

  if (NULL == auto_segment) {
    /*
     * Must be checked after PHP_WRAPPER_CALL to ensure txn didn't end during
     * the call.
     */
    goto end;
  }

  message_segment = nr_segment_start(NRPRG(txn), NULL, NULL);
  if (NULL != message_segment) {
    /* re-use start time from auto_segment started in func_begin */
    message_segment->start_time = auto_segment->start_time;
    nr_segment_message_end(&message_segment, &message_params);
  }

end:
  /*
   * Because we had to strdup values to persist them beyond
   * NR_PHP_WRAPPER_CALL, now we destroy them. There isn't a separate function
   * to destroy all since some of the params are string literals and we don't
   * want to strdup everything if we don't have to. RabbitMQ basic_publish
   * PHP 7.x will only strdup server_address, destination_name, and
   * messaging_destination_routing_key.
   */
  UNDO_PERSISTENCE(message_params.server_address);
  UNDO_PERSISTENCE(message_params.destination_name);
  UNDO_PERSISTENCE(message_params.messaging_destination_routing_key);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : A wrapper to instrument the php-amqplib basic_get.  This
 * retrieves values to populate a message segment.
 * Note:
 * The DT header 'newrelic' will only be considered if both
 * newrelic.distributed_tracing_enabled is enabled and
 * newrelic.distributed_tracing_exclude_newrelic_header is set to false in the
 * INI settings. The W3C headers 'traceparent' and 'tracestate' will will only
 * be considered if newrelic.distributed_tracing_enabled is enabled in the
 * newrelic.ini settings. If settings are correct, it will
 * retrieve the DT headers and, if applicable, apply to the txn.
 *
 * PhpAmqpLib\Channel\AMQPChannel::basic_get
 * Direct access to a queue if no message was available in the queue, return
 * null
 *
 * @param string $queue
 * @param bool $no_ack
 * @param int|null $ticket
 * @throws \PhpAmqpLib\Exception\AMQPTimeoutException if the specified
 * operation timeout was exceeded
 * @return AMQPMessage|null
 */
NR_PHP_WRAPPER(nr_rabbitmq_basic_get) {
  zval* amqp_queue = NULL;
  zval* amqp_exchange = NULL;
  zval* amqp_routing_key = NULL;
  zval* amqp_connection = NULL;
  nr_segment_t* message_segment = NULL;
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  nr_segment_message_params_t message_params = {
      .library = RABBITMQ_LIBRARY_NAME,
      .destination_type = NR_MESSAGE_DESTINATION_TYPE_EXCHANGE,
      .message_action = NR_SPANKIND_CONSUMER,
      .messaging_system = RABBITMQ_MESSAGING_SYSTEM,
  };

  (void)wraprec;

  amqp_queue = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS);
  if (nr_php_is_zval_non_empty_string(amqp_queue)) {
    /* For consumer, this is queue name. */
    message_params.destination_name
        = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_queue));
  }

  amqp_connection = nr_php_get_zval_object_property(
      nr_php_execute_scope(execute_data), "connection");
  /*
   * In PHP 7.x, the following will create a strdup in
   * message_params.server_address that needs to be freed.
   */
  nr_php_amqplib_get_host_and_port(amqp_connection, &message_params);

  /* Compatibility with PHP 7.x */
  NR_PHP_WRAPPER_CALL;

  if (NULL == auto_segment) {
    /*
     * Must be checked after PHP_WRAPPER_CALL to ensure txn didn't end during
     * the call.
     */
    goto end;
  }
  /*
   *The retval should be an AMQPMessage. nr_php_is_zval_* ops do NULL checks
   * as well.
   */
  if (NULL != retval_ptr && nr_php_is_zval_valid_object(*retval_ptr)) {
    /*
     *  Get the exchange and routing key from the AMQPMessage
     */
    amqp_exchange = nr_php_get_zval_object_property(*retval_ptr, "exchange");
    if (nr_php_is_zval_non_empty_string(amqp_exchange)) {
      /* Used with consumer only; this is exchange name.  Exchange name is
       * Default in case of empty string. */
      message_params.messaging_destination_publish_name
          = Z_STRVAL_P(amqp_exchange);
    } else {
      /*
       * For consumer, this is exchange name.  Exchange name is Default in
       * case of empty string.
       */
      if (nr_php_is_zval_valid_string(amqp_exchange)) {
        message_params.messaging_destination_publish_name = "Default";
      }
    }

    amqp_routing_key
        = nr_php_get_zval_object_property(*retval_ptr, "routingKey");
    if (nr_php_is_zval_non_empty_string(amqp_routing_key)) {
      message_params.messaging_destination_routing_key
          = Z_STRVAL_P(amqp_routing_key);
    }

    nr_php_amqplib_retrieve_dt_headers(*retval_ptr);
  }

  /* Now create and end the instrumented segment as a message segment. */
  /*
   * By this point, it's been determined that this call will be instrumented
   * so only create the message segment now, grab the parent segment start
   * time, add our message segment attributes/metrics then close the newly
   * created message segment.
   */
  message_segment = nr_segment_start(NRPRG(txn), NULL, NULL);

  if (NULL == message_segment) {
    goto end;
  }

  /* re-use start time from auto_segment started in func_begin */
  message_segment->start_time = auto_segment->start_time;

  nr_segment_message_end(&message_segment, &message_params);

end:
  /*
   * Because we had to strdup values to persist them beyond
   * NR_PHP_WRAPPER_CALL, now we destroy them. There isn't a separate function
   * to destroy all since some of the params are string literals and we don't
   * want to strdup everything if we don't have to. RabbitMQ basic_get PHP 7.x
   * will only strdup server_address and destination_name.
   */
  UNDO_PERSISTENCE(message_params.server_address);
  UNDO_PERSISTENCE(message_params.destination_name);
}
NR_PHP_WRAPPER_END

void nr_php_amqplib_enable() {
  /*
   * Set the UNKNOWN package first, so it doesn't overwrite what we find with
   * nr_php_amqplib_handle_version.
   */
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }

  /* Extract the version */
  nr_php_amqplib_handle_version();
  nr_php_amqplib_ensure_class();

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* less than PHP8.0 */
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("PhpAmqpLib\\Channel\\AMQPChannel::basic_publish"),
      nr_rabbitmq_basic_publish_before, nr_rabbitmq_basic_publish,
      nr_rabbitmq_basic_publish);

  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("PhpAmqpLib\\Channel\\AMQPChannel::basic_get"), NULL,
      nr_rabbitmq_basic_get, nr_rabbitmq_basic_get);
#else
  nr_php_wrap_user_function(
      NR_PSTR("PhpAmqpLib\\Channel\\AMQPChannel::basic_publish"),
      nr_rabbitmq_basic_publish);

  nr_php_wrap_user_function(
      NR_PSTR("PhpAmqpLib\\Channel\\AMQPChannel::basic_get"),
      nr_rabbitmq_basic_get);
#endif
}
