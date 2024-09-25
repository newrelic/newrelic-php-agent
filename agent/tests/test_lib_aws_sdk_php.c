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

    p = nr_php_packages_get_package(NRPRG(txn)->php_package_suggestions,
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
   * Aws/Sdk class does not exist, should not create package metric suggestion
   * if no version This case should never happen in real situations.
   */
  tlib_php_request_start();

  nr_lib_aws_sdk_php_handle_version();

  p = nr_php_packages_get_package(NRPRG(txn)->php_package_suggestions,
                                  LIBRARY_NAME);

  test_description = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                                "suggestion created");
  tlib_pass_if_not_null(test_description, p);
  nr_free(test_description);

  test_description
      = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                   "suggested version set to PHP_PACKAGE_VERSION_UNKNOWN");
  tlib_pass_if_str_equal(test_description, PHP_PACKAGE_VERSION_UNKNOWN,
                         p->package_version);
  nr_free(test_description);

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
