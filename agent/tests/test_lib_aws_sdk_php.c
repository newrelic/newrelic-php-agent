/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "lib_aws_sdk_php.h"
#include "fw_support.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO > ZEND_7_1_X_API_NO

#define PHP_AWS_CLASS_PREFIX "Supportability/library/aws/aws-sdk-php/"
#define PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX \
  "Supportability/PHP/AWS/Services/"

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
#define LIBRARY_MAJOR_VERSION "7"
#define LIBRARY_MAJOR_VERSION_2 "10"
#define LIBRARY_MAJOR_VERSION_3 "100"
#define LIBRARY_MAJOR_VERSION_4 "4.23"
#define LIBRARY_MAJOR_VERSION_55 "55.34"
#define LIBRARY_MAJOR_VERSION_6123 "6123.45"
#define LIBRARY_MAJOR_VERSION_0 "0.4.5"
#define PACKAGE_METRIC "Supportability/PHP/package/" LIBRARY_NAME

  /*
   * If lib_aws_sdk_php_handle_version function is ever called, we have already
   * detected the aws-sdk-php library.
   */

  /*
   * Aws/Sdk exists. Should return aws package metric with
   * version
   */
  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null("version test 1: package metric created",
                        nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION "/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_2);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null("version test 2: package metric created",
                        nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION_2 "/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_3);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null("version test 3: package metric created",
                        nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION_3 "/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_4);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null(
      "version test 4: package metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC "/4/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_55);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null(
      "version test 5: package metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC "/55/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_6123);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null(
      "version test 6: package metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC "/6123/detected"));
  tlib_php_request_end();

  tlib_php_request_start();
  declare_aws_sdk_class("Aws", "Sdk", LIBRARY_MAJOR_VERSION_0);
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_not_null(
      "version test 7: package metric created",
      nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC "/0/detected"));
  tlib_php_request_end();

  /*
   * Aws/Sdk does not exist, should not return package metric if no version
   * This case should never happen in real situations.
   */
  tlib_php_request_start();
  nr_lib_aws_sdk_php_handle_version();
  tlib_pass_if_null("aws library metric created",
                    nrm_find(NRPRG(txn)->unscoped_metrics, PACKAGE_METRIC));
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_lib_aws_sdk_php_add_supportability_service_metric();
  test_nr_lib_aws_sdk_php_handle_version();
  tlib_php_engine_destroy();
}
#else
void test_main(void* p NRUNUSED) {}
#endif
