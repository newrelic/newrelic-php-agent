/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "nr_attributes.h"
#include "nr_commands.h"
#include "php_agent.h"
#include "php_globals.h"
#include "php_txn_private.h"

#define LIBRARY_NAME "vendor_name/package_name"
#define LIBRARY_VERSION "1.2.3"
#define DEVEL_LIBRARY_VERSION "1.2.x-dev"
#define LIBRARY_MAJOR_VERSION "1"
#define COMPOSER_PACKAGE_VERSION "2.1.3"
#define COMPOSER_MAJOR_VERSION "2"
#define PACKAGE_METRIC_PREFIX "Supportability/PHP/package/"
#define PACKAGE_METRIC PACKAGE_METRIC_PREFIX LIBRARY_NAME
/* nr_php_use_license() requires exactly NR_LICENSE_SIZE (40) characters or
 * it returns NULL regardless of input -- this is a valid-length stand-in,
 * not a real license. */
#define TEST_LICENSE "0123456789012345678901234567890123456789"

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
  txn->composer_info.entry = NULL;

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
   *        9. package with known "dev" version with and suggestion with unknown version
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

  /* reset everything */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);
  nr_php_packages_destroy(&txn->php_packages);
  txn->php_packages = nr_php_packages_create();
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  /* 9. package with known "dev" version with and suggestion with unknown version */
  nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                               PHP_PACKAGE_VERSION_UNKNOWN);
  nr_txn_add_php_package_from_source(txn, LIBRARY_NAME,
                                     DEVEL_LIBRARY_VERSION,
                                     NR_PHP_PACKAGE_SOURCE_COMPOSER);
  nr_php_txn_create_packages_major_metrics(txn);
  tlib_pass_if_int_equal("suggestion with valid 'dev' version, metric created", 1,
                         nrm_table_size(txn->unscoped_metrics));
  tlib_pass_if_not_null(
      "php package suggestion major version with 'dev' is used for 'detected' metric",
      nrm_find(txn->unscoped_metrics,
               PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

  /* reset everything */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(10);
  nr_php_packages_destroy(&txn->php_packages);
  txn->php_packages = nr_php_packages_create();
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  txn->php_package_major_version_metrics_suggestions = nr_php_packages_create();

  /* 10. package missing from txn->php_packages but present in this
   * thread's own composer scan cache (txn->composer_info.entry->packages):
   * the cache is never consulted for this, so the metric falls back to the
   * suggestion's own version instead of the cache's
   */
  {
    nr_composer_thread_entry_t composer_entry = {0};

    composer_entry.packages = nr_php_packages_create();
    nr_php_packages_add_package(composer_entry.packages,
                                nr_php_package_create_with_source(
                                    LIBRARY_NAME, COMPOSER_PACKAGE_VERSION,
                                    NR_PHP_PACKAGE_SOURCE_COMPOSER));
    txn->composer_info.entry = &composer_entry;

    nr_txn_suggest_package_supportability_metric(txn, LIBRARY_NAME,
                                                 LIBRARY_VERSION);
    nr_php_txn_create_packages_major_metrics(txn);
    tlib_pass_if_int_equal(
        "package only in composer scan cache, metric created", 1,
        nrm_table_size(txn->unscoped_metrics));
    tlib_pass_if_not_null(
        "composer scan cache is not consulted, so the suggestion's own "
        "version is used for 'detected' metric",
        nrm_find(txn->unscoped_metrics,
                 PACKAGE_METRIC "/" LIBRARY_MAJOR_VERSION "/detected"));

    txn->composer_info.entry = NULL;
    nr_php_packages_destroy(&composer_entry.packages);
  }

  /* cleanup */
  nr_php_packages_destroy(&txn->php_packages);
  nr_php_packages_destroy(&txn->php_package_major_version_metrics_suggestions);
  nrm_table_destroy(&txn->unscoped_metrics);

  tlib_php_request_end();
}


static void test_nr_php_txn_populate_app_info_identity() {
  nr_app_info_t info;

  tlib_php_request_start();

  /* Test : NULL info doesn't crash. */
  nr_php_txn_populate_app_info_identity(NULL, "myapp", TEST_LICENSE);

  /* Test : explicit non-empty appnames/license pass through unchanged. */
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "myapp", TEST_LICENSE);
  tlib_pass_if_str_equal("explicit appname used", "myapp", info.appname);
  tlib_pass_if_str_equal("explicit license used", TEST_LICENSE, info.license);
  nr_app_info_destroy_fields(&info);

  /* Test : NULL/empty appnames falls back to NRINI(appnames). */
  NRINI(appnames) = "inidefault";
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, NULL, TEST_LICENSE);
  tlib_pass_if_str_equal("appname falls back to INI", "inidefault",
                        info.appname);
  nr_app_info_destroy_fields(&info);

  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "", TEST_LICENSE);
  tlib_pass_if_str_equal("empty appname falls back to INI", "inidefault",
                        info.appname);
  nr_app_info_destroy_fields(&info);

  /* Test : unresolvable license leaves info.license NULL, but the other
   * required fields are still all populated -- the regression guard for
   * the historical bug this whole extraction is named after. */
  NRINI(license) = NULL;
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "myapp", NULL);
  tlib_pass_if_null("unresolvable license is NULL", info.license);
  tlib_pass_if_not_null("environment still populated", info.environment);
  tlib_pass_if_not_null("lang still populated", info.lang);
  tlib_pass_if_not_null("version still populated", info.version);
  tlib_pass_if_not_null("redirect_collector still populated",
                       info.redirect_collector);
  nr_app_info_destroy_fields(&info);

  /* Test : high_security is copied through. */
  NR_PHP_PROCESS_GLOBALS(high_security) = 1;
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "myapp", TEST_LICENSE);
  tlib_pass_if_true("high_security copied through", info.high_security,
                    "expected high_security to be set");
  NR_PHP_PROCESS_GLOBALS(high_security) = 0;
  nr_app_info_destroy_fields(&info);

  /* Test : trace_observer_host is DT-gated. */
  NRINI(distributed_tracing_enabled) = 1;
  NRINI(trace_observer_host) = "observer.example.com";
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "myapp", TEST_LICENSE);
  tlib_pass_if_str_equal("trace_observer_host copied when DT enabled",
                        "observer.example.com", info.trace_observer_host);
  nr_app_info_destroy_fields(&info);

  NRINI(distributed_tracing_enabled) = 0;
  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, "myapp", TEST_LICENSE);
  tlib_pass_if_str_equal("trace_observer_host empty when DT disabled", "",
                        info.trace_observer_host);
  nr_app_info_destroy_fields(&info);

  tlib_php_request_end();
}

/* The response header attributes of the last transaction sent. */
static struct {
  int sent;
  char* content_type;     /* NULL when absent */
  int64_t content_length; /* -1 when absent */
} captured;

static void captured_reset(void) {
  nr_free(captured.content_type);
  nr_memset(&captured, 0, sizeof(captured));
  captured.content_length = -1;
}

static nr_status_t capture_txndata_tx(const nrtxn_t* txn) {
  nrobj_t* attrs = nr_attributes_agent_to_obj(
      txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_EVENT);
  const char* content_type
      = nro_get_hash_string(attrs, "response.headers.contentType", NULL);
  nr_status_t err = NR_FAILURE;
  int64_t content_length
      = nro_get_hash_long(attrs, "response.headers.contentLength", &err);

  captured_reset();
  captured.sent = 1;
  captured.content_type = content_type ? nr_strdup(content_type) : NULL;
  captured.content_length = (NR_SUCCESS == err) ? content_length : -1;

  nro_delete(attrs);
  return NR_SUCCESS;
}

/*
 * Starts a request whose web transaction has a Content-Type of
 * application/xml and a Content-Length of 7 in its response headers.
 */
static void request_start_with_response_headers(void) {
  tlib_php_request_start();
  tlib_php_request_eval(
      "header('Content-Type: application/xml'); header('Content-Length: 7');");
  nr_txn_set_as_web_transaction(NRPRG(txn), "test");
}

static void test_nr_php_txn_end_with_response_headers() {
  nr_status_t (*orig_hook)(const nrtxn_t*) = nr_cmd_txndata_hook;

  nr_cmd_txndata_hook = capture_txndata_tx;

  /* Test : the given values are recorded, not the response headers. */
  captured_reset();
  request_start_with_response_headers();
  tlib_pass_if_status_success(
      "given headers", nr_php_txn_end_with_response_headers("text/plain", 42));
  tlib_pass_if_null("given headers end the transaction", NRPRG(txn));
  tlib_php_request_end();
  tlib_pass_if_int_equal("given headers sent", 1, captured.sent);
  tlib_pass_if_str_equal("given Content-Type", "text/plain",
                         captured.content_type);
  tlib_pass_if_int64_t_equal("given Content-Length", 42,
                             captured.content_length);

  /* Test : a Content-Length of -1 records none. */
  captured_reset();
  request_start_with_response_headers();
  nr_php_txn_end_with_response_headers("text/plain", -1);
  tlib_php_request_end();
  tlib_pass_if_str_equal("no Content-Length: Content-Type", "text/plain",
                         captured.content_type);
  tlib_pass_if_int64_t_equal("no Content-Length", -1, captured.content_length);

  /*
   * Test : a NULL Content-Type records none, and the given Content-Length is
   * still recorded, not the response headers' one.
   */
  captured_reset();
  request_start_with_response_headers();
  nr_php_txn_end_with_response_headers(NULL, 42);
  tlib_php_request_end();
  tlib_pass_if_null("no Content-Type", captured.content_type);
  tlib_pass_if_int64_t_equal("no Content-Type: Content-Length", 42,
                             captured.content_length);

  /* Test : nr_php_txn_end still reads both from the response headers. */
  captured_reset();
  request_start_with_response_headers();
  nr_php_txn_end(0, 0);
  tlib_php_request_end();
  tlib_pass_if_str_equal("nr_php_txn_end: Content-Type", "application/xml",
                         captured.content_type);
  tlib_pass_if_int64_t_equal("nr_php_txn_end: Content-Length", 7,
                             captured.content_length);

  /* Test : a background transaction records neither. */
  captured_reset();
  request_start_with_response_headers();
  nr_txn_set_as_background_job(NRPRG(txn), "test");
  nr_php_txn_end_with_response_headers("text/plain", 42);
  tlib_php_request_end();
  tlib_pass_if_int_equal("background sent", 1, captured.sent);
  tlib_pass_if_null("background: no Content-Type", captured.content_type);
  tlib_pass_if_int64_t_equal("background: no Content-Length", -1,
                             captured.content_length);

  nr_cmd_txndata_hook = orig_hook;
  captured_reset();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_php_txn_php_package_create_major_metric();
  test_nr_php_txn_create_packages_major_metrics();
  /* Before test_nr_php_txn_populate_app_info_identity, which clears the
   * license, so that later requests begin no transaction. */
  test_nr_php_txn_end_with_response_headers();
  test_nr_php_txn_populate_app_info_identity();
  tlib_php_engine_destroy();
}
