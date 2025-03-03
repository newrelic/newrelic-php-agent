/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_wrapper.h"
#include "fw_support.h"
#include "nr_segment_message.h"
#include "lib_aws_sdk_php.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
/*
 * Aside from service class and version detection, instrumentation is only
 * supported with PHP 8.1+
 */

#define ARG_VALUE_FOR_TEST "curly_q"
#define COMMAND_NAME_FOR_TEST "uniquelyAwesome"
#define COMMAND_NAME_LEN_FOR_TEST sizeof(COMMAND_NAME_FOR_TEST) - 1
#define ARG_TO_FIND_FOR_TEST AWS_SDK_PHP_SQSCLIENT_QUEUEURL_ARG
#define AWS_QUEUEURL_LEN_MAX 512

/* These wrappers are used so we don't have to mock up zend_execute_data. */

NR_PHP_WRAPPER(expect_arg_value_not_null) {
  char* command_arg_value = NULL;

  (void)wraprec;

  command_arg_value = nr_lib_aws_sdk_php_get_command_arg_value(
      ARG_TO_FIND_FOR_TEST, NR_EXECUTE_ORIG_ARGS);
  tlib_pass_if_not_null(
      "Expect a valid command_arg_value if a valid named arg exists.",
      command_arg_value);
  tlib_pass_if_str_equal("Arg name/value pair should match.",
                         ARG_VALUE_FOR_TEST, command_arg_value);
  nr_free(command_arg_value);
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(expect_arg_value_null) {
  char* command_arg_value = NULL;

  (void)wraprec;

  command_arg_value = nr_lib_aws_sdk_php_get_command_arg_value(
      ARG_TO_FIND_FOR_TEST, NR_EXECUTE_ORIG_ARGS);
  tlib_pass_if_null(
      "Expect a null command_arg_value if no valid named arg exists.",
      command_arg_value);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(aws_lambda_invoke_wrapper) {
  nr_segment_cloud_attrs_t cloud_attrs = {0};
  /* 
   * Because argument 1 is not used in instrumentation, we will use it
   * to pass in the expected value
   */
  zval* expected = nr_php_get_user_func_arg(1, NR_EXECUTE_ORIG_ARGS);
  nr_aws_sdk_lambda_client_invoke_parse_args(NR_EXECUTE_ORIG_ARGS, &cloud_attrs);
  (void)wraprec;

  if (nr_php_is_zval_valid_string(expected)) {
    tlib_pass_if_str_equal("Expected should match reconstructed arn",
            Z_STRVAL_P(expected),
            cloud_attrs.cloud_resource_id);
  } else {
    tlib_pass_if_str_equal("Expected should match reconstructed arn",
            NULL,
            cloud_attrs.cloud_resource_id);
  }
  NR_PHP_WRAPPER_CALL;
  nr_free(cloud_attrs.cloud_resource_id);
}
NR_PHP_WRAPPER_END

static void test_nr_lib_aws_sdk_php_get_command_arg_value() {
  zval* expr = NULL;
  zval* first_arg = NULL;
  zval* array_arg = NULL;

  /*
   * nr_lib_aws_sdk_php_get_command_arg_value extracts an arg value from the 2nd
   * argument in the argument list, so we need to have at least 2 args to
   * extract properly.
   */
  tlib_php_engine_create("");
  tlib_php_request_start();

  tlib_php_request_eval("function one_param($a) { return; }");
  nr_php_wrap_user_function(NR_PSTR("one_param"), expect_arg_value_null);
  tlib_php_request_eval("function two_param_valid($a, $b) { return; }");
  nr_php_wrap_user_function(NR_PSTR("two_param_valid"),
                            expect_arg_value_not_null);
  tlib_php_request_eval("function two_param($a, $b) { return; }");
  nr_php_wrap_user_function(NR_PSTR("two_param"), expect_arg_value_null);
  tlib_php_request_eval("function no_param() { return;}");
  nr_php_wrap_user_function(NR_PSTR("no_param"), expect_arg_value_null);

  /*
   * The function isn't decoding this arg, so it doesn't matter what it is as
   * long as it exists.
   */
  first_arg = tlib_php_request_eval_expr("1");

  /* Valid case.  The wrapper should verify strings match. */

  char* valid_array_args
      = "array("
        "    0 => array("
        "        'QueueUrl' => 'curly_q'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(valid_array_args);
  expr = nr_php_call(NULL, "two_param_valid", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Test Invalid Cases*/

  /*
   * Invalid case: QueueUrl found but value was not a string.  The wrapper
   * should see the null return value.
   */
  char* queueurl_not_string_arg
      = "array("
        "    0 => array("
        "        'QueueUrl' => array("
        "                      'Nope' => 'curly_q'"
        "         )"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(queueurl_not_string_arg);
  expr = nr_php_call(NULL, "two_param", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Invalid case: only one parameter.  The wrapper should see the null return
   * value. */
  expr = nr_php_call(NULL, "one_param", first_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);

  /* Invalid case: no parameter.  The wrapper should see the null return value.
   */
  expr = nr_php_call(NULL, "no_param");
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);

  /*
   *Invalid case: QueueUrl not found in the argument array.  The wrapper should
   *see the null return value.
   */
  char* no_queueurl_arg
      = "array("
        "    0 => array("
        "        'Nope' => 'curly_q'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(no_queueurl_arg);
  expr = nr_php_call(NULL, "two_param", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /*
   *Invalid case: inner arg in the argument array is not an array.  The wrapper
   *should see the null return value.
   */
  char* arg_in_array_not_array
      = "array("
        "    0 => '1'"
        ")";
  array_arg = tlib_php_request_eval_expr(arg_in_array_not_array);
  expr = nr_php_call(NULL, "two_param", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /*
   *Invalid case: empty argument array.  The wrapper should see
   * the null return value.
   */
  char* no_arg_in_array
      = "array("
        ")";
  array_arg = tlib_php_request_eval_expr(no_arg_in_array);
  expr = nr_php_call(NULL, "two_param", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /*
   *Invalid case: The argument array is not an array.  The wrapper should see
   * the null return value.
   */
  char* array_arg_not_array = "1";
  array_arg = tlib_php_request_eval_expr(array_arg_not_array);
  expr = nr_php_call(NULL, "two_param", first_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  nr_php_zval_free(&first_arg);
  tlib_php_request_end();
  tlib_php_engine_destroy();
}

static inline void test_message_param_queueurl_settings_expect_val(
    nr_segment_message_params_t* message_params,
    nr_segment_cloud_attrs_t* cloud_attrs,
    char* cloud_region,
    char* cloud_account_id,
    char* destination_name) {
  tlib_pass_if_str_equal("cloud_region should match.", cloud_attrs->cloud_region,
                         cloud_region);
  tlib_pass_if_str_equal("cloud_account_id should match.",
                         cloud_attrs->cloud_account_id, cloud_account_id);
  tlib_pass_if_str_equal("destination_name should match.",
                         message_params->destination_name, destination_name);
}

static inline void test_message_param_queueurl_settings_expect_null(
    nr_segment_message_params_t* message_params,
    nr_segment_cloud_attrs_t* cloud_attrs) {
  if (NULL != cloud_attrs) {
    tlib_pass_if_null("cloud_region should be null.", cloud_attrs->cloud_region);
    tlib_pass_if_null("cloud_account_id should be null.",
                      cloud_attrs->cloud_account_id);
  }
  if (NULL != message_params) {
    tlib_pass_if_null("destination_name should be null.",
                      message_params->destination_name);
  }
}

static void test_nr_lib_aws_sdk_php_sqs_parse_queueurl() {
  /*
   * nr_lib_aws_sdk_php_sqs_parse_queueurl extracts either ALL cloud_region,
   * cloud_account_id, and destination_name or none.
   */
  nr_segment_message_params_t message_params = {0};
  nr_segment_cloud_attrs_t cloud_attrs = {0};
  char modifiable_string[AWS_QUEUEURL_LEN_MAX];

  tlib_php_engine_create("");

// clang-format off
#define VALID_QUEUE_URL  "https://sqs.us-east-2.amazonaws.com/123456789012/SQS_QUEUE_NAME"
#define INVALID_QUEUE_URL_1 "https://us-east-2.amazonaws.com/123456789012/SQS_QUEUE_NAME"
#define INVALID_QUEUE_URL_2 "https://sqs.us-east-2.amazonaws.com/123456789012/"
#define INVALID_QUEUE_URL_3 "https://sqs.us-east-2.amazonaws.com/SQS_QUEUE_NAME"
#define INVALID_QUEUE_URL_4 "https://random.com"
#define INVALID_QUEUE_URL_5 "https://sqs.us-east-2.amazonaws.com/123456789012"
#define INVALID_QUEUE_URL_6 "https://sqs.us-east-2.amazonaws.com/"
#define INVALID_QUEUE_URL_7 "https://sqs.us-east-2.amazonaws.com"
#define INVALID_QUEUE_URL_8 "https://sqs.us-east-2.random.com/123456789012/SQS_QUEUE_NAME"
  // clang-format on

  /* Test null queueurl.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(NULL, &message_params, &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test null message_params.  No values extracted, all values should be
   * null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(NULL, NULL, &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test null cloud_attrs.  No values extracted, all values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(NULL, &message_params, NULL);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_1);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_2);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_3);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_4);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_5);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_6);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_7);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /* Test Invalid values.  Extracted message_param values should be null.*/

  nr_strcpy(modifiable_string, INVALID_QUEUE_URL_8);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_null(&message_params, &cloud_attrs);

  /*
   * Test 'https://sqs.us-east-2.amazonaws.com/123456789012/SQS_QUEUE_NAME'.
   * Extracted message_param values should set.
   */

  nr_strcpy(modifiable_string, VALID_QUEUE_URL);
  nr_lib_aws_sdk_php_sqs_parse_queueurl(modifiable_string, &message_params,
                                        &cloud_attrs);
  test_message_param_queueurl_settings_expect_val(&message_params, &cloud_attrs,
                                                  "us-east-2", "123456789012",
                                                  "SQS_QUEUE_NAME");

  tlib_php_engine_destroy();
}
#endif /* PHP 8.1+ */

#if ZEND_MODULE_API_NO > ZEND_7_1_X_API_NO

static void declare_aws_sdk_class(const char* ns,
                                  const char* klass,
                                  const char* sdk_version) {
  char* source = nr_formatf(
      "namespace %s;"
      "class %s{"
      "const VERSION = '%s';"
      "}",
      ns, klass, sdk_version);

  tlib_php_request_eval(source);

  nr_free(source);
}

static void test_nr_lib_aws_sdk_php_add_supportability_service_metric(void) {
  /*
   * Should return aws metric with classname
   */
  tlib_php_request_start();

  int num_metrics = nrm_table_size(NRPRG(txn)->unscoped_metrics);
  nr_lib_aws_sdk_php_add_supportability_service_metric(NULL);
  tlib_pass_if_int_equal(
      "aws supportability metric 0: metric not created in NULL metrics",
      num_metrics, nrm_table_size(NRPRG(txn)->unscoped_metrics));

  nr_lib_aws_sdk_php_add_supportability_service_metric("one\\two");
  tlib_pass_if_not_null(
      "aws supportability metric 1: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "one\\two"));

  nr_lib_aws_sdk_php_add_supportability_service_metric("three\\four");
  tlib_pass_if_not_null(
      "aws supportability metric 2: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "three\\four"));

  nr_lib_aws_sdk_php_add_supportability_service_metric("three\\four\\five");
  tlib_pass_if_not_null(
      "aws supportability metric 3: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "three\\four\\five"));

  nr_lib_aws_sdk_php_add_supportability_service_metric("three\\");
  tlib_pass_if_not_null(
      "aws supportability metric 4: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "three\\"));
  nr_lib_aws_sdk_php_add_supportability_service_metric("\\four");
  tlib_pass_if_not_null(
      "aws supportability metric 5: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "\\four"));
  nr_lib_aws_sdk_php_add_supportability_service_metric("five");
  tlib_pass_if_not_null(
      "aws supportability metric 6: service/client metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics,
               PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX "five"));

  tlib_php_request_end();
}

static void test_nr_lib_aws_sdk_php_handle_version(void) {
#define LIBRARY_NAME "aws/aws-sdk-php"
  const char* library_versions[]
      = {"7", "10", "100", "4.23", "55.34", "6123.45", "0.4.5"};
  nr_php_package_t* p = NULL;
#define TEST_DESCRIPTION_FMT                                                  \
  "nr_lib_aws_sdk_php_handle_version with library_versions[%ld]=%s: package " \
  "major version metric - %s"
  char* test_description = NULL;
  size_t i = 0;

  /*
   * If lib_aws_sdk_php_handle_version function is ever called, we have already
   * detected the aws-sdk-php library.
   */

  /*
   * Aws/Sdk class exists. Should create aws package metric suggestion with
   * version
   */
  for (i = 0; i < sizeof(library_versions) / sizeof(library_versions[0]); i++) {
    tlib_php_request_start();

    declare_aws_sdk_class("Aws", "Sdk", library_versions[i]);
    nr_lib_aws_sdk_php_handle_version();

    p = nr_php_packages_get_package(
        NRPRG(txn)->php_package_major_version_metrics_suggestions,
        LIBRARY_NAME);

    test_description = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                                  "suggestion created");
    tlib_pass_if_not_null(test_description, p);
    nr_free(test_description);

    test_description = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                                  "suggested version set");
    tlib_pass_if_str_equal(test_description, library_versions[i],
                           p->package_version);
    nr_free(test_description);

    tlib_php_request_end();
  }

  /*
   * Aws/Sdk class does not exist, should create package metric suggestion
   * with PHP_PACKAGE_VERSION_UNKNOWN version. This case should never happen
   * in real situations.
   */
  tlib_php_request_start();

  nr_lib_aws_sdk_php_handle_version();

  p = nr_php_packages_get_package(
      NRPRG(txn)->php_package_major_version_metrics_suggestions, LIBRARY_NAME);

  tlib_pass_if_not_null(
      "nr_lib_aws_sdk_php_handle_version when Aws\\Sdk class is not defined - "
      "suggestion created",
      p);
  tlib_pass_if_str_equal(
      "nr_lib_aws_sdk_php_handle_version when Aws\\Sdk class is not defined - "
      "suggested version set to PHP_PACKAGE_VERSION_UNKNOWN",
      PHP_PACKAGE_VERSION_UNKNOWN, p->package_version);

  tlib_php_request_end();
}

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
static void test_nr_lib_aws_sdk_php_lambda_invoke() {
  tlib_php_engine_create("");
  tlib_php_request_start();

  tlib_php_request_eval("function lambda_invoke($a, $b) { return; }");
  nr_php_wrap_user_function(NR_PSTR("lambda_invoke"), aws_lambda_invoke_wrapper);

  NRINI(aws_account_id) = "111122223333";

  /* Test full-info run */
  char* args
      = "array("
        "    0 => array("
        "        'FunctionName' => 'us-east-2:012345678901:function:my-function'"
        "    )"
        ")";
  zval* array_arg = tlib_php_request_eval_expr(args);
  char* expect = "'arn:aws:lambda:us-east-2:012345678901:function:my-function'";
  zval* expect_arg = tlib_php_request_eval_expr(expect);
  zval* expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&expect_arg);
  nr_php_zval_free(&array_arg);

  /* Test alias full-info run */
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => 'us-east-2:012345678901:function:my-function:v1'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expect = "'arn:aws:lambda:us-east-2:012345678901:function:my-function:v1'";
  expect_arg = tlib_php_request_eval_expr(expect);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&expect_arg);
  nr_php_zval_free(&array_arg);

  /* Test INI extract */
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => 'us-east-2:my-function'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expect = "'arn:aws:lambda:us-east-2:111122223333:function:my-function'";
  expect_arg = tlib_php_request_eval_expr(expect);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&expect_arg);
  nr_php_zval_free(&array_arg);

  /* Test failed INI extract */
  NRINI(aws_account_id) = "";
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => 'us-east-2:my-function'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expect = "NULL";
  expect_arg = tlib_php_request_eval_expr(expect);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);
  NRINI(aws_account_id) = "111122223333";

  /* Test NULL INI */
  NRINI(aws_account_id) = NULL;
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => 'us-east-2:my-function'"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expect = "NULL";
  expect_arg = tlib_php_request_eval_expr(expect);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);
  NRINI(aws_account_id) = "111122223333";

  /* Test invalid arg 1 */
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => 123"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Test invalid arg 2 */
  args
      = "array("
        "    0 => array("
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Test invalid arg 3 */
  args = "array()";
  array_arg = tlib_php_request_eval_expr(args);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Test invalid arg 4 */
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => ''"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&array_arg);

  /* Test invalid arg 5 */
  args
      = "array("
        "    0 => array("
        "        'FunctionName' => NULL"
        "    )"
        ")";
  array_arg = tlib_php_request_eval_expr(args);
  expr = nr_php_call(NULL, "lambda_invoke", expect_arg, array_arg);
  tlib_pass_if_not_null("Expression should evaluate.", expr);
  nr_php_zval_free(&expr);
  nr_php_zval_free(&expect_arg);
  nr_php_zval_free(&array_arg);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

#endif /* PHP 8.1+ */

static void test_nr_lib_aws_sdk_ini() {
  /* test too short */
  tlib_php_engine_create("newrelic.cloud.aws.account_id=\"12345678901\"");
  tlib_php_request_start();
  tlib_pass_if_str_equal("Expected short account id to be dropped",
                         NULL, NRINI(aws_account_id));
  tlib_php_request_end();
  tlib_php_engine_destroy();

  /* test too long */
  tlib_php_engine_create("newrelic.cloud.aws.account_id=\"1234567890123\"");
  tlib_php_request_start();
  tlib_pass_if_str_equal("Expected short account id to be dropped",
                         NULL, NRINI(aws_account_id));
  tlib_php_request_end();
  tlib_php_engine_destroy();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_lib_aws_sdk_php_add_supportability_service_metric();
  test_nr_lib_aws_sdk_php_handle_version();
  tlib_php_engine_destroy();
  test_nr_lib_aws_sdk_ini();
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_nr_lib_aws_sdk_php_sqs_parse_queueurl();
  test_nr_lib_aws_sdk_php_get_command_arg_value();
  test_nr_lib_aws_sdk_php_lambda_invoke();
#endif /* PHP 8.1+ */
}
#else
void test_main(void* p NRUNUSED) {}
#endif
