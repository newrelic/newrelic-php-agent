/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_txn_private.h"

#define LIBRARY_NAME "vendor_name/package_name"
#define LIBRARY_VERSION "1.2.3"
#define LIBRARY_MAJOR_VERSION "1"
#define COMPOSER_PACKAGE_VERSION "2.1.3"
#define COMPOSER_MAJOR_VERSION "2"
#define PACKAGE_METRIC_PREFIX "Supportability/PHP/package/"
#define PACKAGE_METRIC PACKAGE_METRIC_PREFIX LIBRARY_NAME

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

nr_php_package_t php_package
    = {.package_name = LIBRARY_NAME,
       .package_version = LIBRARY_VERSION,
       .source_priority = NR_PHP_PACKAGE_SOURCE_COMPOSER};

static void test_nr_php_txn_php_package_create_major_metric() {
  nrtxn_t t;
  nrtxn_t* txn = &t;

  txn->unscoped_metrics = nrm_table_create(10);
  txn->php_packages = nr_php_packages_create();
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  tlib_php_request_start();

  /* need to call callback with invalid values to make sure it doesnt crash
   * code depends on txn and txn->php_packages existing so these are created
   * above with the package suggestions data structure included for good measure
   */

  /* this tests these params in callback:
   * suggested = NULL
   * actual = NULL
   * key = NULL
   * txn = NULL
   */
  nr_php_txn_php_package_create_major_metric(NULL, NULL, 0, NULL);
  tlib_pass_if_int_equal("NULL txn, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* this tests these params in callback:
   * suggested = NULL
   * actual = NULL
   * key != NULL
   * txn != NULL
   */
  nr_php_txn_php_package_create_major_metric(NULL, LIBRARY_NAME,
                                             strlen(LIBRARY_NAME), (void*)txn);
  tlib_pass_if_int_equal("NULL value, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* the key is not actually used by the callback - just the package name
   * in the suggested package so this casee will still create a metric
   */
  /* this tests these params in callback:
   * suggested != NULL
   * actual = NULL
   * key = NULL
   * txn = != NULL
   */
  nr_php_txn_php_package_create_major_metric(&php_package, NULL, 0, (void*)txn);
  tlib_pass_if_int_equal("NULL key, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));

  /* cleanup */
  nr_php_packages_destroy(&txn->php_packages);
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  nrm_table_destroy(&txn->unscoped_metrics);

  tlib_php_request_end();
}

static void test_nr_php_txn_create_packages_major_metrics() {
  nrtxn_t t;
  nrtxn_t* txn = &t;

  txn->unscoped_metrics = nrm_table_create(10);
  txn->php_packages = nr_php_packages_create();
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  tlib_php_request_start();

  /* invalid txn should not crash */
  nr_php_txn_create_packages_major_metrics(NULL);
  tlib_pass_if_int_equal("NULL txn, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* test with valid txn no package suggestions */
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("valid txn with no suggestions, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /*
   * Tests:
   *        1. suggestion with NULL version, no packages
   *        2. suggestion with PHP_PACKAGE_VERSION_UNKNOWN version, no packages
   *        3. suggestion with known version, no packages
   *        4. package with known version and suggestion with known version
   *        5. package with known version and suggestion with unknown version
   *        6. package with unknown version and suggestion with known version
   *        7. package with unknown version and suggestion with unknown version
   *        8. test that causes "actual" to be NULL in callback
   */

  /* 1. suggestion with NULL version, no packages */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME, NULL);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with NULL version, metric not created", 0,
                         nrm_table_size(txn->unscoped_metrics));

  /* 2. suggestion with PHP_PACKAGE_VERSION_UNKNOWN version, no packages
   * also
   * 8. test that causes "actual" to be NULL in callback
   */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               PHP_PACKAGE_VERSION_UNKNOWN);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal(
      "suggestion with PHP_PACKAGE_VERSION_UNKNOWN version, metric not created",
      0, nrm_table_size(txn->unscoped_metrics));

  /* 3. suggestion with known version, no packages
   * also
   * 8. test that causes "actual" to be NULL in callback
   */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               LIBRARY_VERSION);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with valid version, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));
  tlib_pass_if_not_null(
      "php package major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

  /* reset metrics */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);

  /* 4. package with known version and suggestion with known version
   *
   * add a package with a "better" version determined from composer api
   * and use existing suggestion which has a different version
   */
  nr_txn_add_php_package_from_source(txn, LIBRARY_NAME,
                                     COMPOSER_PACKAGE_VERSION,
                                     NR_PHP_PACKAGE_SOURCE_COMPOSER);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with valid version, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));
  tlib_pass_if_not_null(
      "php package major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" COMPOSER_MAJOR_VERSION "/detected"));

  /* reset suggestions, leave package with known version in place */
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  /* reset metrics */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);

  /* 5. package with known version and suggestion with unknown version
   *
   * add a suggestion with no version and test metric uses package version
   */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               PHP_PACKAGE_VERSION_UNKNOWN);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with valid version, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));
  tlib_pass_if_not_null(
      "php package major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" COMPOSER_MAJOR_VERSION "/detected"));

  /* reset everything */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);
  nr_php_packages_destroy(&txn->php_packages);
  txn->php_packages = nr_php_packages_create();
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  /* 6. package with unknown version and suggestion with known version */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               LIBRARY_VERSION);
  nr_txn_add_php_package_from_source(txn, LIBRARY_NAME,
                                     PHP_PACKAGE_VERSION_UNKNOWN,
                                     NR_PHP_PACKAGE_SOURCE_COMPOSER);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with valid version, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));
  tlib_pass_if_not_null(
      "php package suggestion major version is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

  /* reset everything */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);
  nr_php_packages_destroy(&txn->php_packages);
  txn->php_packages = nr_php_packages_create();
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  /* 7. package with unknown version and suggestion with unknown version */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               PHP_PACKAGE_VERSION_UNKNOWN);
  nr_txn_add_php_package_from_source(txn, LIBRARY_NAME,
                                     PHP_PACKAGE_VERSION_UNKNOWN,
                                     NR_PHP_PACKAGE_SOURCE_COMPOSER);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal(
      "suggestion and package w/o version, metric not created", 0,
      nrm_table_size(txn->unscoped_metrics));

  /* cleanup */
  nr_php_packages_destroy(&txn->php_packages);
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  nrm_table_destroy(&txn->unscoped_metrics);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_php_txn_php_package_create_major_metric();
  test_nr_php_txn_create_packages_major_metrics();
  tlib_php_engine_destroy();
}
