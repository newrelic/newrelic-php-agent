/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "fw_support.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

// When package detection for vulnerability management is disbled,
// txn->php_packages is not populated and package version cannot
// be obtained from php_package. This test is to ensure that
// the package supportability metric is created in case php_package
// is not available and that fallback version is used.
static void test_fw_supportability_metrics_with_vm_disabled(void) {
#define LIBRARY_NAME "php-package"
#define LIBRARY_MAJOR_VERSION "7"
#define LIBRARY_MAJOR_VERSION_2 "10"
#define LIBRARY_MAJOR_VERSION_3 "100"
#define LIBRARY_MAJOR_VERSION_4 "1.23"
#define LIBRARY_MAJOR_VERSION_5 "12.34"
#define LIBRARY_MAJOR_VERSION_6 "123.45"
#define LIBRARY_MAJOR_VERSION_7 "0.4.5"
#define LIBRARY_METRIC "Supportability/library/" LIBRARY_NAME "/detected"
#define LOGGING_LIBRARY_METRIC "Supportability/Logging/PHP/" LIBRARY_NAME
#define PACKAGE_METRIC_PREFIX "Supportability/PHP/package/"
#define PACKAGE_METRIC PACKAGE_METRIC_PREFIX LIBRARY_NAME
  nrtxn_t t;
  nrtxn_t* txn = &t;
  nr_php_package_t* php_package = NULL;
  txn->unscoped_metrics = nrm_table_create(10);

  /* NULL tests - don't blow up */
  nr_fw_support_add_library_supportability_metric(NULL, LIBRARY_NAME);
  tlib_pass_if_int_equal("library metric not created in NULL metrics", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_library_supportability_metric(txn, NULL);
  tlib_pass_if_int_equal("NULL library metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_logging_supportability_metric(NULL, LIBRARY_NAME, true);
  tlib_pass_if_int_equal("logging library metric not created in NULL metrics",
                         0, nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_logging_supportability_metric(txn, NULL, true);
  tlib_pass_if_int_equal("NULL logging library metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_package_supportability_metric(
      NULL, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, php_package);
  tlib_pass_if_int_equal("package metric not created in NULL metrics",
                         0, nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_package_supportability_metric(
      txn, NULL, LIBRARY_MAJOR_VERSION, php_package);
  tlib_pass_if_int_equal("NULL package name, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  nr_fw_support_add_package_supportability_metric(txn, LIBRARY_NAME, NULL,
                                                  php_package);
  tlib_pass_if_int_equal("NULL major version, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* Happy path */
  nr_fw_support_add_library_supportability_metric(txn, LIBRARY_NAME);
  tlib_pass_if_not_null("happy path: library metric created",
                        nrm_find(txn->unscoped_metrics, LIBRARY_METRIC));

  nr_fw_support_add_logging_supportability_metric(txn, LIBRARY_NAME, true);
  tlib_pass_if_not_null(
      "happy path: logging library metric created",
      nrm_find(txn->unscoped_metrics, LOGGING_LIBRARY_METRIC "/enabled"));

  nr_fw_support_add_logging_supportability_metric(txn, LIBRARY_NAME, false);
  tlib_pass_if_not_null(
      "happy path: logging library metric created",
      nrm_find(txn->unscoped_metrics, LOGGING_LIBRARY_METRIC "/disabled"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, php_package);
  tlib_pass_if_not_null("happy path test 1: package metric created",
                        nrm_find(txn->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION "/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_2, php_package);
  tlib_pass_if_not_null("happy path test 2: package metric created",
                        nrm_find(txn->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION_2 "/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_3, php_package);
  tlib_pass_if_not_null("happy path test 3: package metric created",
                        nrm_find(txn->unscoped_metrics, PACKAGE_METRIC
                                 "/" LIBRARY_MAJOR_VERSION_3 "/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_4, php_package);
  tlib_pass_if_not_null(
      "happy path test 4: package metric created",
      nrm_find(txn->unscoped_metrics, PACKAGE_METRIC "/1/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_5, php_package);
  tlib_pass_if_not_null(
      "happy path test 5: package metric created",
      nrm_find(txn->unscoped_metrics, PACKAGE_METRIC "/12/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_6, php_package);
  tlib_pass_if_not_null(
      "happy path test 6: package metric created",
      nrm_find(txn->unscoped_metrics, PACKAGE_METRIC "/123/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION_7, php_package);
  tlib_pass_if_not_null(
      "happy path test 7: package metric created",
      nrm_find(txn->unscoped_metrics, PACKAGE_METRIC "/0/detected"));

  NRINI(force_framework) = true;
  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, php_package);
  tlib_pass_if_not_null(
      "happy path test 8: package metric created",
      nrm_find(txn->unscoped_metrics, PACKAGE_METRIC "/7/forced"));

  nrm_table_destroy(&txn->unscoped_metrics);
}

// When package detection for vulnerability management is enabled,
// txn->php_packages is populated and package version can be obtained
// from php_package stored in txn->php_packages. This test is to ensure
// that the package supportability metric is created in case php_package
// is available and that package version from php_package is used.
static void test_fw_supportability_metrics_with_vm_enabled(void) {
#define PHP_PACKAGE_MAJOR_VERSION "8"
#define PHP_PACKAGE_VERSION PHP_PACKAGE_MAJOR_VERSION ".4.0"
  nrtxn_t t;
  nrtxn_t* txn = &t;
  nr_php_package_t php_package
      = {.package_name = LIBRARY_NAME,
         .package_version = PHP_PACKAGE_VERSION,
         .source_priority = NR_PHP_PACKAGE_SOURCE_COMPOSER};
  nr_php_package_t php_package_null_version
      = {.package_name = LIBRARY_NAME,
         .package_version = NULL,
         .source_priority = NR_PHP_PACKAGE_SOURCE_COMPOSER};
  nr_php_package_t php_package_unknown_version
      = {.package_name = LIBRARY_NAME,
         .package_version = PHP_PACKAGE_VERSION_UNKNOWN,
         .source_priority = NR_PHP_PACKAGE_SOURCE_COMPOSER};
  txn->unscoped_metrics = nrm_table_create(10);

  NRINI(force_framework) = false;
  nr_fw_support_add_package_supportability_metric(txn, LIBRARY_NAME, NULL,
                                                  &php_package_null_version);
  tlib_pass_if_null(
      "library major version metric not created when version is unknown - "
      "version is NULL and package version is NULL",
      nrm_get_metric(txn->unscoped_metrics, 0));

  nr_fw_support_add_package_supportability_metric(txn, LIBRARY_NAME,
                                                  PHP_PACKAGE_VERSION_UNKNOWN,
                                                  &php_package_null_version);
  tlib_pass_if_null(
      "library major version metric not created when version is unknown - "
      "version is PHP_PACKAGE_VERSION_UNKNOWN and package version is NULL",
      nrm_get_metric(txn->unscoped_metrics, 0));

  nr_fw_support_add_package_supportability_metric(txn, LIBRARY_NAME, NULL,
                                                  &php_package_unknown_version);
  tlib_pass_if_null(
      "library major version metric not created when version is unknown - "
      "version is NULL and package version is PHP_PACKAGE_VERSION_UNKNOWN",
      nrm_get_metric(txn->unscoped_metrics, 0));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, &php_package);
  tlib_pass_if_not_null(
      "php package major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" PHP_PACKAGE_MAJOR_VERSION "/detected"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, &php_package_null_version);
  tlib_pass_if_not_null(
      "library major version is used for 'detected' metric when php package "
      "version is NULL",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

  NRINI(force_framework) = true;
  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, &php_package);
  tlib_pass_if_not_null(
      "php package major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" PHP_PACKAGE_MAJOR_VERSION "/forced"));

  nr_fw_support_add_package_supportability_metric(
      txn, LIBRARY_NAME, LIBRARY_MAJOR_VERSION, &php_package_null_version);
  tlib_pass_if_not_null(
      "library major version is used for 'forced' metric when php package "
      "version is NULL",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

  nrm_table_destroy(&txn->unscoped_metrics);
}

void test_main(void* p NRUNUSED) {
  test_fw_supportability_metrics_with_vm_disabled();
  test_fw_supportability_metrics_with_vm_enabled();
}
