/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting AWS-SDK-PHP.
 */
#ifndef LIB_AWS_SDK_PHP_HDR
#define LIB_AWS_SDK_PHP_HDR

#include "nr_segment_message.h"
#include "nr_segment_external.h"
#include "nr_datastore_instance.h"
#include "nr_segment_datastore.h"
#include "nr_segment_datastore_private.h"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO /* PHP8.1+ */
/* Service instrumentation only supported above PHP 8.1+*/

/* SQS */
#define SQS_LIBRARY_NAME "SQS"
#define AWS_SQS_MESSAGING_SERVICE "aws_sqs"
#define AWS_SDK_PHP_SQSCLIENT_QUEUEURL_ARG "QueueUrl"
#define AWS_QUEUEURL_PREFIX "https://sqs."
#define AWS_QUEUEURL_PREFIX_LEN sizeof(AWS_QUEUEURL_PREFIX) - 1
#define AWS_QUEUEURL_AWS_POSTFIX "amazonaws.com/"
#define AWS_QUEUEURL_AWS_POSTFIX_LEN sizeof(AWS_QUEUEURL_AWS_POSTFIX) - 1

/* DynamoDb */
#define AWS_SDK_PHP_DYNAMODBCLIENT_DATASTORE_SYSTEM "dynamodb"
#define AWS_SDK_PHP_DYNAMODBCLIENT_TABLENAME_ARG "TableName"
/* DynamoDb uses a set host then does magic under the hood to resolve the
 * connection. */
#define AWS_SDK_PHP_DYNAMODBCLIENT_DEFAULT_HOST "dynamodb.amazonaws.com"
#define AWS_SDK_PHP_DYNAMODBCLIENT_DEFAULT_PORT "8000"
/* Spec defined format for the operations so all agents will send consistent
 * strings. */
#define AWS_SDK_PHP_DYNAMODBCLIENT_CREATE_TABLE "create_table"
#define AWS_SDK_PHP_DYNAMODBCLIENT_DELETE_ITEM "delete_item"
#define AWS_SDK_PHP_DYNAMODBCLIENT_DELETE_TABLE "delete_table"
#define AWS_SDK_PHP_DYNAMODBCLIENT_GET_ITEM "get_item"
#define AWS_SDK_PHP_DYNAMODBCLIENT_PUT_ITEM "put_item"
#define AWS_SDK_PHP_DYNAMODBCLIENT_QUERY "query"
#define AWS_SDK_PHP_DYNAMODBCLIENT_SCAN "scan"
#define AWS_SDK_PHP_DYNAMODBCLIENT_UPDATE_ITEM "update_item"

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
 * Params  : 1. The QueueUrl, MUST be a modifiable string
 *           2. message_params to set message_params.destination_name
 *           3. cloud_attrs to set message_params.cloud_region,
 * message_params.cloud_account_id
 *
 * Returns : applicable cloud_attrs and message params fields will point to null
 * terminated strings within the original string.
 *
 */
extern void nr_lib_aws_sdk_php_sqs_parse_queueurl(
    char* sqs_queueurl,
    nr_segment_message_params_t* message_params,
    nr_segment_cloud_attrs_t* cloud_attrs);

/*
 * Purpose : Handle when an SqsClient initiates a command
 *
 * Params  : 1. segment : if we instrument the commandName, we'll need to end
 * the segment as a message segment
 *           2. command_name_string : the string of the command being called
 *           3. command_name_len : the length of the command being called
 *           4. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 *
 * Returns :
 *
 */
extern void nr_lib_aws_sdk_php_sqs_handle(nr_segment_t* segment,
                                          char* command_name_string,
                                          size_t command_name_len,
                                          NR_EXECUTE_PROTO);

/*
 * Purpose : Handle when a LambdaClient::invoke command happens
 *
 * Params  : 1. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 *           2. cloud_attrs : the cloud attributes pointer to be
 *              populated with the ARN
 *
 * Returns :
 *
 * Note: The caller is responsible for freeing cloud_attrs->cloud_resource_id
 */
void nr_aws_sdk_lambda_client_invoke_parse_args(
    NR_EXECUTE_PROTO,
    nr_segment_cloud_attrs_t* cloud_attrs);

/*
 * Purpose : Handles regex destruction during mshutdown
 */
void nr_aws_sdk_mshutdown(void);

/*
 * Purpose : The second argument to the Aws/AwsClient::__call function should be
 * an array, the first element of which is itself an array of arguments that
 * were passed to the called function and are in name:value pairs. Given an
 * argument name, this will return the value of the argument.
 *
 * Params  : 1. arg_name: name of argument to extract from command arg array
 *           2. NR_EXECUTE_PROTO (execute_data, func_return_value)
 *
 * Returns : the strduped value of the arg_name; NULL if does not exist
 *
 * Note: The caller is responsible for freeing the returned string value
 *
 */
extern char* nr_lib_aws_sdk_php_get_command_arg_value(char* command_arg_name,
                                                      NR_EXECUTE_PROTO);

/*
 * Purpose : Handle when a DynamoDbClient initiates a command
 *
 * Params  : 1. segment : end the segment as a datastore segment
 *           2. command_name_string : the string of the command being called
 *           3. command_name_len : the length of the command being called
 *           4. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 *
 * Returns :
 *
 */
extern void nr_lib_aws_sdk_php_dynamodb_handle(nr_segment_t* segment,
                                               char* command_name_string,
                                               size_t command_name_len,
                                               NR_EXECUTE_PROTO);

/*
 * Purpose : Populate DynamoDbClient datastore_params and cloud_attrs.  This
 will extract region and host/port and set the value in the appropriate struct.
             Note: A host/port other than the AWS DynamoDb default is very
 uncommon and used only for development environments
 *
 * Params  : 1. datastore_params : datastore params to modify
 *           2. cloud_attrs : cloud attributes to modify
 *           3. NR_EXECUTE_ORIG_ARGS (execute_data, func_return_value)
 * Returns :
 * Note: caller is responsible for freeing the cloud_attrs->cloud_resource_id
 and datastore_params->collection and datastore_params.instance.port_path_or_id
 */
extern void nr_lib_aws_sdk_php_dynamodb_set_params(
    nr_segment_datastore_params_t* datastore_params,
    nr_segment_cloud_attrs_t* cloud_attrs,
    NR_EXECUTE_PROTO);

#endif /* PHP8.1+ */

#endif /* LIB_AWS_SDK_PHP_HDR */
