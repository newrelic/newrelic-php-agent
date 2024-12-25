/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
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

static inline void test_message_param_queueurl_settings_expect_val(
    nr_segment_message_params_t message_params,
    char* cloud_region,
    char* cloud_account_id,
    char* destination_name) {
  tlib_pass_if_str_equal("cloud_region should match.",
                         message_params.cloud_region, cloud_region);
  tlib_pass_if_str_equal("cloud_account_id should match.",
                         message_params.cloud_account_id, cloud_account_id);
  tlib_pass_if_str_equal("destination_name should match.",
                         message_params.destination_name, destination_name);

  nr_free(message_params.cloud_region);
  nr_free(message_params.cloud_account_id);
  nr_free(message_params.destination_name);
}

static inline void test_message_param_queueurl_settings_expect_null(
    nr_segment_message_params_t message_params) {
  tlib_pass_if_null("cloud_region should be null.",
                    message_params.cloud_region);
  tlib_pass_if_null("cloud_account_id should be null.",
                    message_params.cloud_account_id);
  tlib_pass_if_null("destination_name should be null.",
                    message_params.destination_name);
}

static void test_nr_lib_aws_sdk_php_sqs_parse_queueurl() {
  /*
   * nr_lib_aws_sdk_php_sqs_parse_queueurl extracts either ALL cloud_region,
   * cloud_account_id, and destination_name or none.
   */
  nr_segment_message_params_t message_params = {0};

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
  nr_lib_aws_sdk_php_sqs_parse_queueurl(NULL, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_1, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_2, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_3, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_4, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_5, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_6, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_7, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /* Test Invalid values.  Extracted message_param values should be null.*/
  nr_lib_aws_sdk_php_sqs_parse_queueurl(INVALID_QUEUE_URL_8, &message_params);
  test_message_param_queueurl_settings_expect_null(message_params);

  /*
   * Test 'https://sqs.us-east-2.amazonaws.com/123456789012/SQS_QUEUE_NAME'.
   * Extracted message_param values should set.
   */
  nr_lib_aws_sdk_php_sqs_parse_queueurl(VALID_QUEUE_URL, &message_params);
  test_message_param_queueurl_settings_expect_val(
      message_params, "us-east-2", "123456789012", "SQS_QUEUE_NAME");
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

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_lib_aws_sdk_php_add_supportability_service_metric();
  test_nr_lib_aws_sdk_php_handle_version();
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  test_nr_lib_aws_sdk_php_sqs_parse_queueurl();
#endif /* PHP 8.1+ */
  tlib_php_engine_destroy();
}
#else
void test_main(void* p NRUNUSED) {}
#endif
