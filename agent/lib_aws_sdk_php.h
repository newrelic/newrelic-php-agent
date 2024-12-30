/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting AWS-SDK-PHP.
 */
#ifndef LIB_AWS_SDK_PHP_HDR
#define LIB_AWS_SDK_PHP_HDR

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Service instrumentation only supported above PHP 8.1+*/
#define SQS_LIBRARY_NAME "SQS"
#define AWS_SQS_MESSAGING_SERVICE "aws_sqs"
#define AWS_SDK_PHP_SQSCLIENT_CLASS "Aws\\Sqs\\SqsClient"
#define AWS_SDK_PHP_SQSCLIENT_QUEUEURL_ARG "QueueUrl"
#define AWS_SDK_PHP_DYNAMODBCLIENT_CLASS "Aws\\DynamoDb\\DynamoDbClient"
#define AWS_QUEUEURL_LEN_MAX 512
#endif /* PHP 8.1+ */

#define PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX \
  "Supportability/PHP/AWS/Services/"
#define MAX_METRIC_NAME_LEN 256
#define PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN \
  sizeof(PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX)
#define MAX_AWS_SERVICE_NAME_LEN \
  (MAX_METRIC_NAME_LEN - PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN)

extern void nr_aws_sdk_php_enable();
extern void nr_lib_aws_sdk_php_handle_version();
extern void nr_lib_aws_sdk_php_add_supportability_service_metric(
    const char* service_name);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Aside from service class and version detection, instrumentation is only
 * supported with PHP 8.1+ */

/*
 * Purpose : Parses the QueueUrl to extract cloud_region, cloud_account_id, and
 * destination_name.  The extraction sets all or none since the values are from
 * the same string and if it is malformed, it cannot be used.
 *
 * Params  : 1. The QueueUrl
 *           2. message_params to set message_params.destination_name
 *           3. cloud_attrs to set message_params.cloud_region,
 * message_params.cloud_account_id
 *
 * Returns :
 *
 * Note: caller is responsible for
 * freeing cloud_attrs.cloud_region, cloud_attrs.cloud_account_id, and
 * message_params.destination_name
 */
extern void nr_lib_aws_sdk_php_sqs_parse_queueurl(
    const char* sqs_queueurl,
    nr_segment_message_params_t* message_params,
    nr_segment_cloud_attrs_t* cloud_attrs);

/*
 * Purpose : Handle when an SqsClient initiates a command
 *
 * Params  : 1. segment : if we instrument the commandName, we'll need to end
 * the segment as a message segment
 *           2. command_name_string : the string of the command being called
 *           3. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 * Returns :
 *
 */
extern void nr_lib_aws_sdk_php_sqs_handle(nr_segment_t* segment,
                                          char* command_name_string,
                                          NR_EXECUTE_PROTO);

/*
 * Purpose : The second argument to the Aws/AwsClient::__call function should be
 * an array containing an array of argument name:value pairs. Given an argument
 * name, this will return the value of the argument.
 *
 * Params  : 1. arg_name: name of argument to extract from command arg array
 *           2. NR_EXECUTE_PROTO (execute_data, func_return_value)
 *
 * Returns : the value of the arg_name; NULL if does not exist
 *
 * Note: The caller is responsible for freeing the returned string value
 *
 */
extern char* nr_lib_aws_sdk_php_get_command_arg_value(char* command_arg_name,
                                                      NR_EXECUTE_PROTO);

/*
 * Purpose : Return the first argument to the Aws/AwsClient::__call function
 * which should be the name of the command.
 *
 * Params  : 1. NR_EXECUTE_PROTO (execute_data, func_return_value)
 *
 * Returns : the value of the command_name; NULL if does not exist
 *
 * Note: The caller is responsible for freeing the returned string value
 *
 */
extern char* nr_lib_aws_sdk_php_get_command_name(NR_EXECUTE_PROTO);
#endif /* PHP8.1+ */

#endif /* LIB_AWS_SDK_PHP_HDR */
