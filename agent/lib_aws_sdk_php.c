/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the AWS-SDK-PHP.
 * https://github.com/aws/aws-sdk-php
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "nr_segment_message.h"
#include "lib_aws_sdk_php.h"

#define PHP_PACKAGE_NAME "aws/aws-sdk-php"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Service instrumentation only supported above PHP 8.1+*/

/*
* Note: For SQS, the command_arg_array will contain the following arrays seen
below:
//clang-format off
$result = $client->receiveMessage(array(
    // QueueUrl is required
    'QueueUrl' => 'string',
    'AttributeNames' => array('string', ... ),
    'MessageAttributeNames' => array('string', ... ),
    'MaxNumberOfMessages' => integer,
    'VisibilityTimeout' => integer,
    'WaitTimeSeconds' => integer,
));

$result = $client->sendMessage(array(
    // QueueUrl is required
    'QueueUrl' => 'string',
    // MessageBody is required
    'MessageBody' => 'string',
    'DelaySeconds' => integer,
    'MessageAttributes' => array(
        // Associative array of custom 'String' key names
        'String' => array(
            'StringValue' => 'string',
            'BinaryValue' => 'string',
            'StringListValues' => array('string', ... ),
            'BinaryListValues' => array('string', ... ),
            // DataType is required
            'DataType' => 'string',
        ),
        // ... repeated
    ),
));


$result = $client->sendMessageBatch(array(
    // QueueUrl is required
    'QueueUrl' => 'string',
    // Entries is required
    'Entries' => array(
        array(
            // Id is required
            'Id' => 'string',
            // MessageBody is required
            'MessageBody' => 'string',
            'DelaySeconds' => integer,
            'MessageAttributes' => array(
                // Associative array of custom 'String' key names
                'String' => array(
                    'StringValue' => 'string',
                    'BinaryValue' => 'string',
                    'StringListValues' => array('string', ... ),
                    'BinaryListValues' => array('string', ... ),
                    // DataType is required
                    'DataType' => 'string',
                ),
                // ... repeated
            ),
        ),
        // ... repeated
    ),
));

//clang-format on
*/

char* nr_lib_aws_sdk_php_get_command_name(NR_EXECUTE_PROTO) {
  zval* command_name = NULL;
  char* command_name_string = NULL;

  /* get the command_name. */
  command_name = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS);

  if (0 != nr_php_is_zval_non_empty_string(command_name)) {
    command_name_string = nr_strdup(Z_STRVAL_P(command_name));
  }

  nr_php_arg_release(&command_name);
  return command_name_string;
}

void nr_lib_aws_sdk_php_sqs_handle(nr_segment_t* segment, NR_EXECUTE_PROTO) {
  char* command_arg_value = NULL;
  char* command_name_string = NULL;
  bool instrumented = false;
  nr_segment_message_params_t message_params = {
      .library = "SQS",
      .destination_type = NR_MESSAGE_DESTINATION_TYPE_QUEUE,
      .messaging_system = "aws_sqs",
  };
  nr_segment_cloud_attrs_t cloud_attrs = {0};

  if (NULL == segment) {
    return;
  }

  command_name_string
      = nr_lib_aws_sdk_php_get_command_name(NR_EXECUTE_ORIG_ARGS);

  /* Determine if we instrument this command. */
  if (nr_streq(command_name_string, "sendMessage")
      || nr_streq(command_name_string, "sendMessageBatch")) {
    message_params.message_action = NR_SPAN_PRODUCER;
    instrumented = true;
  } else if (nr_streq(command_name_string, "receiveMessage")) {
    message_params.message_action = NR_SPAN_CONSUMER;
    instrumented = true;
  }

  if (instrumented) {
    cloud_attrs.aws_operation = command_name_string;

    command_arg_value = nr_lib_aws_sdk_php_get_command_arg_value(
        AWS_SDK_PHP_SQSCLIENT_QUEUEURL_ARG, NR_EXECUTE_ORIG_ARGS);

    /*
     *  nr_lib_aws_sdk_php_sqs_parse_queueurl checks for NULL so safe pass
     * command_arg_value directly in.
     */
    nr_lib_aws_sdk_php_sqs_parse_queueurl(command_arg_value, &message_params,
                                          &cloud_attrs);

    /* Add cloud attributes, if available. */

    nr_segment_traces_add_cloud_attributes(segment, &cloud_attrs);

    /* Now end the instrumented segment as a message segment. */
    nr_segment_message_end(&segment, &message_params);
  }

  nr_free(command_arg_value);
  nr_free(command_name_string);

  /*
   * These are the message_params params that were strduped in
   * nr_lib_aws_sdk_php_sqs_parse_queueurl
   */
  nr_free(cloud_attrs.cloud_region);
  nr_free(message_params.destination_name);
  nr_free(cloud_attrs.cloud_account_id);
}

void nr_lib_aws_sdk_php_sqs_parse_queueurl(
    const char* sqs_queueurl,
    nr_segment_message_params_t* message_params,
    nr_segment_cloud_attrs_t* cloud_attrs) {
  char* region = NULL;
  char* queue_name = NULL;
  char* account_id = NULL;
  char queueurl[AWS_QUEUEURL_LEN_MAX];
  char* queueurl_pointer = NULL;

  if (NULL == sqs_queueurl || NULL == message_params) {
    return;
  }

  /*
   * AWS QueueUrl has a very specific format.
   * The QueueUrl we are looking for will be of the following format:
   * queueUrl =
   * 'https://sqs.REGION_NAME.amazonaws.com/ACCOUNT_ID_NAME/SQS_QUEUE_NAME'
   * where REGION_NAME, ACCOUNT_ID_NAME, and SQS_QUEUE_NAME are the acutal
   * values such as: queueUrl =
   * 'https://sqs.us-east-2.amazonaws.com/123456789012/my_amazing_queue'
   * If we are unable to match any part of this, the whole decode is suspect and
   * all values are discarded.
   *
   * Due to the overhead involved in escaping the original buffer, creating a
   * regex, matching a regex, destroying a regex, this method was chosen as a
   * more performant option because it's a very limited pattern.
   */

  if (NULL == nr_strlcpy(queueurl, sqs_queueurl, AWS_QUEUEURL_LEN_MAX)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }
  queueurl_pointer = queueurl;

  /*
   * Find the pattern of the AWS queueurl that should immediately precede the
   * region.
   */
  if (0 != strncmp(queueurl_pointer, "https://sqs.", 12)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  /*
   * Find the start of the region.  It follows the 12 chars of 'https://sqs.'
   * and continues until the next '.' It is safe to move the pointer along at
   * this point since we allocated a sufficiently big buffer.
   */
  queueurl_pointer += 12;
  if (nr_strempty(queueurl_pointer)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  region = queueurl_pointer;

  /* Find the end of the region. */
  queueurl_pointer = nr_strchr(queueurl_pointer, '.');
  if (NULL == queueurl_pointer) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }
  *queueurl_pointer = '\0';

  /*
   * Move the pointer along. Again, we found a valid '.' so moving the pointer
   * beyond that point should be safe and give us either more string or the end
   * of the string.
   */
  queueurl_pointer += 1;
  if (nr_strempty(queueurl_pointer)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  /* Move past the next pattern to find the start of the account id. */
  if (0 != strncmp(queueurl_pointer, "amazonaws.com/", 14)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  /*
   * Move the pointer along. Since we found a valid pattern match moving the
   * pointer beyond that point should be safe and give us either more string or
   * the end of the string.
   */
  queueurl_pointer += 14;
  if (nr_strempty(queueurl_pointer)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  /* If it's not an empty string, we've found the start of the account_id*/
  account_id = queueurl_pointer;

  /* Find the end of account id which goes until the next forward slash. */
  queueurl_pointer = nr_strchr(queueurl_pointer, '/');
  if (NULL == queueurl_pointer) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }
  *queueurl_pointer = '\0';

  /* Move the pointer along. */
  queueurl_pointer += 1;
  if (nr_strempty(queueurl_pointer)) {
    /* Malformed queueurl, we can't decode this. */
    return;
  }

  /* This should be the start of the start of the queuename.*/
  queue_name = queueurl_pointer;

  /*
   * Almost done. At this point, the string should only have queue name left.
   * Let's check if there's another slash, if it isn't followed by empty string,
   * the queueurl is malformed.
   */
  queueurl_pointer = nr_strchr(queueurl_pointer, '/');
  if (NULL != queueurl_pointer) {
    *queueurl_pointer = '\0';
    /* Let's check if it's followed by empty string */
    *queueurl_pointer += 1;
    if (!nr_strempty(queueurl_pointer)) {
      /* Malformed queueurl, we can't decode this. */
      return;
    }
  }

  /*
   * SQS entity relationship requires: messaging.system, cloud.region,
   * cloud.account.id, messaging.destination.name
   */
  message_params->destination_name = nr_strdup(queue_name);
  cloud_attrs->cloud_account_id = nr_strdup(account_id);
  cloud_attrs->cloud_region = nr_strdup(region);
}

char* nr_lib_aws_sdk_php_get_command_arg_value(char* command_arg_name,
                                               NR_EXECUTE_PROTO) {
  zval* param_array = NULL;
  zval* command_arg_array = NULL;
  char* command_arg_value = NULL;

  if (NULL == command_arg_name) {
    return NULL;
  }
  /* To extract the Aws/AwsClient::__call $argument, we get the second arg. */
  param_array = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS);

  if (nr_php_is_zval_valid_array(param_array)) {
    /* The first element in param_array is an array of parameters. */
    command_arg_array = nr_php_zend_hash_index_find(Z_ARRVAL_P(param_array), 0);
    if (nr_php_is_zval_valid_array(command_arg_array)) {
      zval* queueurl_arg = nr_php_zend_hash_find(Z_ARRVAL_P(command_arg_array),
                                                 command_arg_name);

      if (nr_php_is_zval_non_empty_string(queueurl_arg)) {
        command_arg_value = nr_strdup(Z_STRVAL_P(queueurl_arg));
      }
    }
  }

  nr_php_arg_release(&param_array);
  return command_arg_value;
}

/*
 * For Aws/AwsClient::__call see
 * https://github.com/aws/aws-sdk-php/blob/master/src/AwsClientInterface.php
 * ALL
 * client commands are handled by this function, so it is the start and end of
 * any command. Creates and executes a command for an operation by name.
 * When a class command isn't explicitly created as a function, the __call class
 * handles the invocation.  This means all AWS Client Service commands are
 * handled by this call. Any invocation starts when this function starts, and
 * ends when it ends.  This function decodes the command name, determines the
 * appropriate args, decodes the args, generates a guzzle request to send to the
 * AWS Service, gets the guzzle response from the AWS Service, and bundles that
 * response into an AswResult to return.
 *
 * @param string $name      Name of the command to execute.
 * @param array  $arguments Arguments to pass to the getCommand method.
 *
 * @return ResultInterface
 * @throws \Exception
 */
NR_PHP_WRAPPER(nr_aws_client_call) {
  (void)wraprec;

  const char* klass = NULL;
  klass
      = nr_php_class_entry_name(Z_OBJCE_P(nr_php_execute_scope(execute_data)));
  if (nr_streq(klass, AWS_SDK_PHP_SQSCLIENT_CLASS)) {
    nr_lib_aws_sdk_php_sqs_handle(auto_segment, NR_EXECUTE_ORIG_ARGS);
  }
}
NR_PHP_WRAPPER_END

#endif /* PHP >= 8.1*/
/*
 * In a normal course of events, the following line will always work
 * zend_eval_string("Aws\\Sdk::VERSION;", &retval, "Get AWS Version")
 * By the time we have detected the existence of the aws-sdk-php and with
 * default composer project settings, it is callable even from
 * nr_aws_sdk_php_enable which will automatically load the class if it isn't
 * loaded yet and then evaluate the string. In the rare case that files
 * are not loaded via autoloader and/or have non-default composer classload
 * settings, if the class is not found, PHP 8.2+ will generate an
 * error whenever it cannot find a class which must be caught. Calling this
 * from nr_aws_sdk_php_enable would allow the sdk version value to be set only
 * once. To avoid the VERY unlikely but not impossible fatal error, we need to
 * wrap the call in a try/catch block and make it a lambda so that we avoid
 * fatal errors.
 */
void nr_lib_aws_sdk_php_handle_version() {
  char* version = NULL;
  zval retval;
  int result = FAILURE;

  result = zend_eval_string(
      "(function() {"
      "     $nr_aws_sdk_version = '';"
      "     try {"
      "          $nr_aws_sdk_version = Aws\\Sdk::VERSION;"
      "     } catch (Throwable $e) {"
      "     }"
      "     return $nr_aws_sdk_version;"
      "})();",
      &retval, "Get nr_aws_sdk_version");

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

void nr_lib_aws_sdk_php_add_supportability_service_metric(
    const char* service_name) {
  /* total MAX metric name length per agent-specs */
  char buf[MAX_METRIC_NAME_LEN];
  char* cp = NULL;

  if (nr_strempty(service_name)) {
    return;
  }
  if (NULL == NRPRG(txn)) {
    return;
  }

  cp = buf;
  nr_strcpy(cp, PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX);
  cp += PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN - 1;
  nr_strlcpy(cp, service_name, MAX_AWS_SERVICE_NAME_LEN);
  nrm_force_add(NRPRG(txn) ? NRTXN(unscoped_metrics) : 0, buf, 0);
}

/*
 * AwsClient::parseClass
 * This is called from the base AwsClient class for every client associated
 * with a service during client initialization.
 * parseClass already computes the service name for internal use, so we don't
 * need to store it, we just need to snag it from the return value as it goes
 * through the client initialization process.
 */
NR_PHP_WRAPPER(nr_create_aws_service_metric) {
  (void)wraprec;

  zval** ret_val_ptr = NULL;
  ret_val_ptr = NR_GET_RETURN_VALUE_PTR;

  NR_PHP_WRAPPER_CALL;

  if (NULL != ret_val_ptr && nr_php_is_zval_valid_array(*ret_val_ptr)) {
    /* obtain ret_val_ptr[0] which contains the service name */
    zval* service_name
        = nr_php_zend_hash_index_find(Z_ARRVAL_P(*ret_val_ptr), 0);
    if (nr_php_is_zval_non_empty_string(service_name)) {
      nr_lib_aws_sdk_php_add_supportability_service_metric(
          Z_STRVAL_P(service_name));
    }
  }
}
NR_PHP_WRAPPER_END

/*
 * The ideal file to begin immediate detection of the aws-sdk is:
 * aws-sdk-php/src/functions.php
 * Unfortunately, Php8.2+ and composer autoload leads to the
 * file being optimized directly and not loaded.
 *
 * Options considered:
 *
 * 1. for PHP8.2, and only optimizable libraries, when encountering autoload.php
 * files, ask the file what includes it added and check against only the
 * optimizable library. Small overhead incurred when encountering an autoload
 * file, but detects aws-sdk-php immediately before any sdk code executes
 * (changes needed for this are detailed in the original PR)
 * 2. use a file that gets called later and only when AwsClient.php file is
 * called. It's called later and we'll miss some instrumentation, but if we're
 * only ever going to be interested in Client calls anyway, maybe that's ok?
 * Doesn't detect Sdk.php (optimized out) so when customers only use that or
 * when they use it first, we will not instrument it. This only detects when a
 * Client is called to use a service so potentially misses out on other
 * instrumentation and misses out when customers use the aws-sdk-php but use
 * non-SDK way to interact with the service (possibly with redis/memcached).
 * This way is definitely the least complex and lowest overhead and less
 * complexity means lower risk as well.
 * 3. Directly add the wrappers to the hash map. With potentially 50ish clients
 * to wrap, this will add overhead to every hash map lookup. Currently
 * implemented option is 2, use the AwsClient.php as this is our main focus.
 * This means until a call to an Aws/AwsClient function,
 * all calls including aws\sdk calls are ignored.
 *
 * Version detection will be called directly from Aws\Sdk.php
 */
void nr_aws_sdk_php_enable() {
  /*
   * Set the UNKNOWN package first, so it doesn't overwrite what we find with
   * nr_lib_aws_sdk_php_handle_version.
   */
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }

  /* Extract the version for aws-sdk 3+ */
  nr_lib_aws_sdk_php_handle_version();

  /* Called when initializing all Clients */
  nr_php_wrap_user_function(NR_PSTR("Aws\\AwsClient::parseClass"),
                            nr_create_aws_service_metric);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
  /* We only support instrumentation above PHP 8.1 */
  /* Called when a service command is issued from a Client */
  nr_php_wrap_user_function_before_after_clean(
      NR_PSTR("Aws\\AwsClient::__call"), NULL, nr_aws_client_call,
      nr_aws_client_call);
#endif
}
