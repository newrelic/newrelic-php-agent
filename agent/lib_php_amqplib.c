/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the php-ampqlib
 * https://github.com/php-amqplib/php-amqplib
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "lib_php_amqplib.h"
#include "nr_segment_message.h"

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
#else
#define ENSURE_PERSISTENCE(x) nr_strdup(x)
#endif

/*
 * Version detection will be called directly from PhpAmqpLib\\Package::VERSION
 * nr_php_amqplib_handle_version will automatically load the class if it isn't
 * loaded yet and then evaluate the string. To avoid the VERY unlikely but not
 * impossible fatal error if the file/class isn't loaded yet, we need to wrap
 * the call in a try/catch block and make it a lambda so that we avoid fatal
 * errors.
 */
void nr_php_amqplib_handle_version() {
  char* version = NULL;
  zval retval;
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
      &retval, "Get nr_php_amqplib_version");

  /* See if we got a non-empty/non-null string for version. */
  if (SUCCESS == result) {
    if (nr_php_is_zval_non_empty_string(&retval)) {
      version = Z_STRVAL(retval);
    }
  }

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Add php package to transaction */
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }

  nr_txn_suggest_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                               version);

  zval_dtor(&retval);
}

static inline void nr_php_amqplib_get_host_and_port(
    zval* amqp_connection,
    nr_segment_message_params_t* message_params) {
  zval* amqp_server = NULL;
  zval* amqp_port = NULL;
  zval* connect_constructor_params = NULL;

  if (NULL == amqp_connection || NULL == message_params) {
    return;
  }

  if (nr_php_is_zval_valid_object(amqp_connection)) {
    connect_constructor_params
        = nr_php_get_zval_object_property(amqp_connection, "construct_params");
    if (nr_php_is_zval_valid_array(connect_constructor_params)) {
      amqp_server
          = nr_php_zend_hash_index_find(Z_ARRVAL_P(connect_constructor_params),
                                        AMQP_CONSTRUCT_PARAMS_SERVER_INDEX);
      if (nr_php_is_zval_non_empty_string(amqp_server)) {
        message_params->server_address
            = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_server));
      }
      amqp_port
          = nr_php_zend_hash_index_find(Z_ARRVAL_P(connect_constructor_params),
                                        AMQP_CONSTRUCT_PARAMS_PORT_INDEX);
      if (nr_php_is_zval_valid_scalar(amqp_port)) {
        message_params->server_port = Z_LVAL_P(amqp_port);
      }
    }
  }
}

/*
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

  amqp_exchange = nr_php_get_user_func_arg(2, NR_EXECUTE_ORIG_ARGS);
  if (nr_php_is_zval_non_empty_string(amqp_exchange)) {
    /*
     * In PHP 7.x, the following will create a strdup in
     * message_params.destination_name that needs to be freed.
     */
    message_params.destination_name
        = ENSURE_PERSISTENCE(Z_STRVAL_P(amqp_exchange));
  } else {
    /* For producer, this is exchange name.  Exchange name is Default in case of
     * empty string. */
    message_params.destination_name = ENSURE_PERSISTENCE("Default");
  }

  amqp_routing_key = nr_php_get_user_func_arg(3, NR_EXECUTE_ORIG_ARGS);
  if (nr_php_is_zval_non_empty_string(amqp_routing_key)) {
    /*
     * In PHP 7.x, the following will create a strdup in
     * message_params.messaging_destination_routing_key that needs to be freed.
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
   * By this point, it's been determined that this call will be instrumented so
   * only create the message segment now, grab the parent segment start time,
   * add our message segment attributes/metrics then close the newly created
   * message segment.
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
    /* PHP 8+ */
    /* gnu compiler needed closure. */
    ;
#else
  /*
   * Because we had to strdup values to persist them beyond NR_PHP_WRAPPER_CALL,
   * now we destroy them. There isn't a separate function to destroy all since
   * some of the params are string literals and we don't want to strdup
   * everything if we don't have to. RabbitMQ basic_publish PHP 7.x will only
   * strdup server_address, destination_name, and
   * messaging_destination_routing_key.
   */
  nr_free(message_params.server_address);
  nr_free(message_params.destination_name);
  nr_free(message_params.messaging_destination_routing_key);
#endif
}
NR_PHP_WRAPPER_END

/*
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
  if (nr_php_is_zval_valid_object(*retval_ptr)) {
    /*
     *  Get the exchange and routing key from the AMQPMessage
     */
    amqp_exchange = nr_php_get_zval_object_property(*retval_ptr, "exchange");
    if (nr_php_is_zval_non_empty_string(amqp_exchange)) {
      /* Used with consumer only; his is exchange name.  Exchange name is
       * Default in case of empty string. */
      message_params.messaging_destination_publish_name
          = Z_STRVAL_P(amqp_exchange);
    } else {
      message_params.messaging_destination_publish_name = "Default";
    }

    amqp_routing_key
        = nr_php_get_zval_object_property(*retval_ptr, "routingKey");
    if (nr_php_is_zval_non_empty_string(amqp_routing_key)) {
      message_params.messaging_destination_routing_key
          = Z_STRVAL_P(amqp_routing_key);
    }
  }

  /* Now create and end the instrumented segment as a message segment. */
  /*
   * By this point, it's been determined that this call will be instrumented so
   * only create the message segment now, grab the parent segment start time,
   * add our message segment attributes/metrics then close the newly created
   * message segment.
   */
  message_segment = nr_segment_start(NRPRG(txn), NULL, NULL);

  if (NULL == message_segment) {
    goto end;
  }

  /* re-use start time from auto_segment started in func_begin */
  message_segment->start_time = auto_segment->start_time;

  nr_segment_message_end(&message_segment, &message_params);

end:
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
    /* PHP 8+ */
    /* gnu compiler needed closure. */
    ;
#else
  /*
   * Because we had to strdup values to persist them beyond NR_PHP_WRAPPER_CALL,
   * now we destroy them. There isn't a separate function to destroy all since
   * some of the params are string literals and we don't want to strdup
   * everything if we don't have to. RabbitMQ basic_get PHP 7.x will only strdup
   * server_address and destination_name.
   */
  nr_free(message_params.server_address);
  nr_free(message_params.destination_name);
#endif
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

  /* Extract the version for aws-sdk 3+ */
  nr_php_amqplib_handle_version();

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* less than PHP8.0 */
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("PhpAmqpLib\\Channel\\AMQPChannel::basic_publish"), NULL,
      nr_rabbitmq_basic_publish, nr_rabbitmq_basic_publish);

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
