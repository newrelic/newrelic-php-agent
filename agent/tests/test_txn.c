/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_header.h"
#include "php_txn_private.h"
#include "php_newrelic.h"

#include "php_globals.h"

static void test_handle_fpm_error(TSRMLS_D) {
  nrobj_t* agent_attributes;
  char* sapi_name;
  /*
   * Test : Bad parameters.
   */
  nr_php_txn_handle_fpm_error(NULL TSRMLS_CC);

  /*
   * Test : Non-FPM. By default, the unit tests report the SAPI as embed, so
   *        the PHP-FPM behavior won't be triggered.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * The next few tests will all pretend to be FPM, so let's set the SAPI name
   * accordingly.
   */
  sapi_name = sapi_module.name;
  sapi_module.name = "fpm-fcgi";

  /*
   * Test : FPM, but with at least one frame called.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
  // file execution no longer increments execute_count on PHPs 8.0+
  // only user function calls do increment execute_count:
  tlib_php_request_eval("function f() {$a = 1 + 1;}\n"
                        "f(); // create a PHP call frame" TSRMLS_CC);
#else
  tlib_php_request_eval("$a = 1 + 1; // create a PHP call frame" TSRMLS_CC);
#endif
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_URI, (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, but with a non-URI path set.
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be unchanged", "foo",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be unchanged",
                         (int)NR_PATH_TYPE_ACTION,
                         (int)NRTXN(status).path_type);

  tlib_php_request_end();

  /*
   * Test : FPM, with the specific case that should result in a status code
   *        based transaction name: a fallback URI path, plus a zero call count
   *        in PHP (since no user function or file is ever executed in the
   *        request).
   */
  tlib_php_request_start();

  nr_txn_set_path(NULL, NRPRG(txn), "foo", NR_PATH_TYPE_URI,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_sapi_headers(TSRMLS_C)->http_response_code = 404;
  nr_php_txn_handle_fpm_error(NRPRG(txn) TSRMLS_CC);
  tlib_pass_if_str_equal("transaction path should be updated", "404",
                         NRTXN(path));
  tlib_pass_if_int_equal("transaction path type should be updated",
                         (int)NR_PATH_TYPE_STATUS_CODE,
                         (int)NRTXN(status).path_type);

  agent_attributes = nr_attributes_agent_to_obj(NRTXN(attributes),
                                                NR_ATTRIBUTE_DESTINATION_ALL);
  tlib_pass_if_not_null("agent attributes must be defined", agent_attributes);
  tlib_pass_if_str_equal(
      "agent attributes must include a request.uri with the original path",
      "foo", nro_get_hash_string(agent_attributes, "request.uri", NULL));
  nro_delete(agent_attributes);

  tlib_php_request_end();

  /*
   * Put the SAPI name back to what it should be.
   */
  sapi_module.name = sapi_name;
}

static void test_max_segments_config_values(TSRMLS_D) {
  nrtxn_t* txn;
  /*
   * Test : max_segments_cli defaults correctly.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_pass_if_size_t_equal("max_segments should be the default of 100,000",
                            100000, txn->options.max_segments);
  tlib_php_request_end();

  /*
   * Test : max_segments_cli gets set correctly.
   */
  NRINI(tt_max_segments_cli) = 200;
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  tlib_php_request_start();

  txn = NRPRG(txn);

  tlib_pass_if_size_t_equal("max_segments should be 200", 200,
                            txn->options.max_segments);

  tlib_php_request_end();

  /*
   * Test : correctly defaults to 0 when web txn.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_pass_if_size_t_equal("max_segments 0 when it's a web txn", 0,
                            txn->options.max_segments);

  tlib_php_request_end();

  /*
   * Test : max_segments_cli does not change web txn max segments.
   */
  NRINI(tt_max_segments_web) = 400;
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  tlib_php_request_start();

  txn = NRPRG(txn);

  tlib_pass_if_size_t_equal("max_segments should be set by web when a web txn",
                            400, txn->options.max_segments);

  tlib_php_request_end();
}

#define PHP_VERSION_METRIC_BASE "Supportability/PHP/Version"
#define AGENT_VERSION_METRIC_BASE "Supportability/PHP/AgentVersion"

static void test_create_php_version_metric() {
  nrtxn_t* txn;
  int count;

  tlib_php_request_start();
  txn = NRPRG(txn);

  count = nrm_table_size(txn->unscoped_metrics);

  /* Test invalid values are properly handled */
  nr_php_txn_create_php_version_metric(NULL, NULL);
  tlib_pass_if_int_equal("PHP version metric shouldnt be created 1", count,
                         nrm_table_size(txn->unscoped_metrics));

  nr_php_txn_create_php_version_metric(txn, NULL);
  tlib_pass_if_int_equal("PHP version metric shouldnt be created 2", count,
                         nrm_table_size(txn->unscoped_metrics));

  nr_php_txn_create_php_version_metric(NULL, "7.4.0");
  tlib_pass_if_int_equal("PHP version metric shouldnt be created 3", count,
                         nrm_table_size(txn->unscoped_metrics));

  nr_php_txn_create_php_version_metric(txn, "");
  tlib_pass_if_int_equal("PHP version metric shouldnt be created 4", count,
                         nrm_table_size(txn->unscoped_metrics));

  /* test valid values */
  nr_php_txn_create_php_version_metric(txn, "7.4.0");
  tlib_pass_if_int_equal("PHP version metric should be create", count + 1,
                         nrm_table_size(txn->unscoped_metrics));

  const nrmetric_t* metric
      = nrm_find(txn->unscoped_metrics, PHP_VERSION_METRIC_BASE "/7.4.0");
  const char* metric_name = nrm_get_name(txn->unscoped_metrics, metric);

  tlib_pass_if_not_null("PHP version metric found", metric);
  tlib_pass_if_str_equal("PHP version metric name check", metric_name,
                         PHP_VERSION_METRIC_BASE "/7.4.0");

  tlib_php_request_end();
}

static void test_create_agent_version_metric() {
  nrtxn_t* txn;
  int count;

  tlib_php_request_start();
  txn = NRPRG(txn);

  count = nrm_table_size(txn->unscoped_metrics);

  /* Test invalid values are properly handled */
  nr_php_txn_create_agent_version_metric(NULL);
  tlib_pass_if_int_equal("Agent version metric shouldnt be created - txn is NULL", count,
                         nrm_table_size(txn->unscoped_metrics));

  /* Test valid values */
  nr_php_txn_create_agent_version_metric(txn);
  tlib_pass_if_int_equal("Agent version metric should be created - txn is not NULL", count + 1,
                         nrm_table_size(txn->unscoped_metrics));

  const nrmetric_t* metric
      = nrm_find(txn->unscoped_metrics, AGENT_VERSION_METRIC_BASE "/" NR_VERSION);
  const char* metric_name = nrm_get_name(txn->unscoped_metrics, metric);

  tlib_pass_if_not_null("Agent version metric found", metric);
  tlib_pass_if_str_equal("Agent version metric name check", metric_name,
                         AGENT_VERSION_METRIC_BASE "/" NR_VERSION);

  tlib_php_request_end();
}

static void test_create_agent_php_version_metrics() {
  nrtxn_t* txn;

  /*
   * Test : Create agent PHP version metrics.
   */
  tlib_php_request_start();
  txn = NRPRG(txn);

  zval* expected_php_zval = tlib_php_request_eval_expr("phpversion();");

  char* php_version_name = nr_formatf(PHP_VERSION_METRIC_BASE "/%s",
                                      Z_STRVAL_P(expected_php_zval));

  nr_php_zval_free(&expected_php_zval);

  char* agent_version_name
      = nr_formatf(AGENT_VERSION_METRIC_BASE "/%s", NR_VERSION);

  nr_php_txn_create_agent_php_version_metrics(txn);

  /* Test the PHP version metric creation */
  const nrmetric_t* metric = nrm_find(txn->unscoped_metrics, php_version_name);
  const char* metric_name = nrm_get_name(txn->unscoped_metrics, metric);

  tlib_pass_if_not_null("happy path: PHP version metric created", metric);
  tlib_pass_if_not_null("happy path: PHP version metric name created",
                        metric_name);

  tlib_pass_if_str_equal("happy path: PHP version metric name check",
                         metric_name, php_version_name);

  /* Test the agent version metric creation*/
  metric = nrm_find(txn->unscoped_metrics, agent_version_name);
  metric_name = nrm_get_name(txn->unscoped_metrics, metric);

  tlib_pass_if_not_null("happy path: Agent version metric created", metric);
  tlib_pass_if_not_null("happy path: Agent version metric name created",
                        metric_name);

  tlib_pass_if_str_equal("happy path: Agent version metric name check",
                         metric_name, agent_version_name);

  nr_free(agent_version_name);
  nr_free(php_version_name);

  tlib_php_request_end();
}

#undef PHP_VERSION_METRIC_BASE
#undef AGENT_VERSION_METRIC_BASE

static void test_create_log_forwarding_labels(TSRMLS_D) {
  nrobj_t* labels = NULL;
  nrobj_t* log_labels = NULL;
  char* json = NULL;

  /* Test : Create log forwarding labels with valid key/value pairs */
  labels = nro_new_hash();
  nro_set_hash_string(labels, "key1", "value1");
  nro_set_hash_string(labels, "key2", "value2");
  nro_set_hash_string(labels, "key3", "value3");

  log_labels = nr_php_txn_get_log_forwarding_labels(labels);

  json = nro_to_json(labels);
  tlib_pass_if_str_equal(
      "valid log label creation test",
      "{\"key1\":\"value1\",\"key2\":\"value2\",\"key3\":\"value3\"}", json);

  nr_free(json);
  nro_delete(labels);
  nro_delete(log_labels);

  /* Test : Create log forwarding labels with empty key/value pairs */
  labels = nro_new_hash();
  nro_set_hash_string(labels, "", "");
  nro_set_hash_string(labels, "key", "");
  nro_set_hash_string(labels, "", "value");

  log_labels = nr_php_txn_get_log_forwarding_labels(labels);

  json = nro_to_json(labels);
  tlib_pass_if_str_equal("empty string log label creation test",
                         "{\"key\":\"\"}", json);

  nr_free(json);
  nro_delete(labels);
  nro_delete(log_labels);

  /* Test : Create log forwarding labels with NULL key/value pairs */
  labels = nro_new_hash();
  nro_set_hash_string(labels, NULL, NULL);
  nro_set_hash_string(labels, "key", NULL);
  nro_set_hash_string(labels, NULL, "value");

  log_labels = nr_php_txn_get_log_forwarding_labels(labels);

  json = nro_to_json(labels);
  tlib_pass_if_str_equal("NULL value log label creation test", "{\"key\":\"\"}",
                         json);

  nr_free(json);
  nro_delete(labels);
  nro_delete(log_labels);

  /* Test : Create log forwarding labels NULL labels object */
  log_labels = nr_php_txn_get_log_forwarding_labels(NULL);
  json = nro_to_json(labels);
  tlib_pass_if_str_equal("NULL object log label creation test", "null", json);

  nr_free(json);
  nro_delete(log_labels);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {

  /*
   * We're setting up our own engine instance because we need to control the
   * attribute configuration.
   */
  // clang-format off
  tlib_php_engine_create("newrelic.transaction_events.attributes.include=request.uri" PTSRMLS_CC);
  // clang-format on

  test_handle_fpm_error();
  test_max_segments_config_values();
  test_create_php_version_metric();
  test_create_agent_version_metric();
  test_create_agent_php_version_metrics();
  test_create_log_forwarding_labels();

  tlib_php_engine_destroy();
}
