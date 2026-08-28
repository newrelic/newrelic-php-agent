/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8.0+ */
#include "nr_commands.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_user_instrument_wraprec_hashmap.h"
#include "nr_php_packages.h"

/*
 * Baseline: agent is disabled at the process level:
 *  - Observer handlers do not get installed
 *  - Agent does not attempt to detect libraries
 *  - Agent does not create any wraprecs
 *  - Agent does not execute any special instrumentation
 */
static void test_register_handlers_globally_disabled(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;

  tlib_php_engine_create("newrelic.enabled=false");
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_false("Agent globally disabled", NR_PHP_PROCESS_GLOBALS(enabled),
                     "Expected agent to be globally disabled");
  tlib_pass_if_null("NRPRG(txn) was not created", NRPRG(txn));

  // Trigger Predis detection
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  // Not much to test when agent is disabled but at least check that wraprec
  // was not created:
  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_null("Predis\\Client::__construct wraprec not created", wr);

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

static nr_status_t mock_cmd_appinfo_unknown(int daemon_fd NRUNUSED,
                                            nrapp_t* app) {
  app->state = NR_APP_UNKNOWN;
  return NR_SUCCESS;
}

/*
 * When the agent is enabled but the current transaction is not recording,
 * here emulated by explicitly setting app status to unknown, Observer
 * handlers still get installed, but not trigger special instrumentation,
 * because they still gate instrumentation on the live nr_php_recording() state.
 */
static void test_register_handlers_enabled_app_unknown(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;
  size_t execute_count;

  tlib_php_engine_create("");
  // Emulate 'first transaction' case by forcing appinfo unknown which in turn
  // causes nr_php_recording() to return 0 because NRPRG(txn) is NULL. This is a
  // valid state for the agent to be in.
  nr_cmd_appinfo_hook = mock_cmd_appinfo_unknown;
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_true("Agent globally enabled", NR_PHP_PROCESS_GLOBALS(enabled),
                    "Expected agent to be globally enabled");
  tlib_pass_if_null("NRPRG(txn) was not created", NRPRG(txn));

  // Trigger library detection
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_not_null("Predis\\Client::__construct wraprec created", wr);

  execute_count = NRTXNGLOBAL(execute_count);

  // Call auto-instrumented function:
  tlib_php_request_eval("new Predis\\Client();");

  // Because agent is not recording, observer handlers should not be invoked.
  // Thus nr_php_instrument_func_begin/nr_php_instrument_func_end should not
  // execute:
  tlib_pass_if_size_t_equal(
      "when not recording, func_begin/func_end should not execute",
      execute_count, NRTXNGLOBAL(execute_count));

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

/*
 * When the agent is enabled but the current transaction is not recording,
 * here emulated by explicitly setting status to not recording, Observer
 * handlers still get installed, but not trigger special instrumentation,
 * because they still gate instrumentation on the live nr_php_recording() state.
 */
static void test_register_handlers_enabled_txn_ignored(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;
  int metric_count = 0;
  size_t execute_count;
  nr_php_package_t* package = NULL;

  tlib_php_engine_create("");
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_true("Agent globally enabled", NR_PHP_PROCESS_GLOBALS(enabled),
                    "Expected agent to be globally enabled");
  // Force nr_php_recording to return 0 by ignoring transaction. This leaves
  // NRPRG(txn) non-NULL but in a state that is not recording.
  tlib_php_request_eval("newrelic_ignore_transaction();");
  tlib_pass_if_not_null("NRPRG(txn) was created", NRPRG(txn));
  tlib_pass_if_true("Transaction is not being recorded", !nr_php_recording(),
                    "Expected transaction to be ignored");

  metric_count = nrm_table_size(NRPRG(txn)->unscoped_metrics);

  // Trigger library detection
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  tlib_pass_if_int_equal("library detected metric created", metric_count + 1,
                         nrm_table_size(NRPRG(txn)->unscoped_metrics));
  tlib_pass_if_not_null("library detected metric created",
                        nrm_find(NRPRG(txn)->unscoped_metrics,
                                 "Supportability/library/Predis/detected"));
  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_not_null("Predis\\Client::__construct wraprec created", wr);

  execute_count = NRTXNGLOBAL(execute_count);

  // Call auto-instrumented function:
  tlib_php_request_eval("new Predis\\Client();");

  // Because agent is not recording, observer handlers should not be invoked.
  // Thus nr_php_instrument_func_begin/nr_php_instrument_func_end should not
  // execute:
  tlib_pass_if_size_t_equal(
      "when not recording, func_begin/func_end should not execute",
      execute_count, NRTXNGLOBAL(execute_count));

  // Because agent is not recording, instrumentation should not be invoked.
  // **Note** : Predis instrumentation side effect is package version metric
  // creation and that fact is used to test if instrumentation was invoked.
  package = nr_php_packages_get_package(
      NRPRG(txn)->php_package_major_version_metrics_suggestions,
      "predis/predis");
  tlib_pass_if_null("when not recording, package version metric not created",
                    package);

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

/*
 * Baseline: agent enabled, transaction recording. Observer handlers get
 * installed, and trigger special instrumentation. Included for matrix
 * completeness alongside the two tests above.
 */
static void test_register_handlers_enabled_and_recording(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;
  int metric_count = 0;
  size_t execute_count = 0;
  nr_php_package_t* package = NULL;

  tlib_php_engine_create("");
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_true("Agent globally enabled", NR_PHP_PROCESS_GLOBALS(enabled),
                    "Expected agent to be globally enabled");
  tlib_pass_if_not_null("NRPRG(txn) was created", NRPRG(txn));
  tlib_pass_if_true("Transaction is being recorded", nr_php_recording(),
                    "Expected transaction to be recorded");

  metric_count = nrm_table_size(NRPRG(txn)->unscoped_metrics);
  /* Trigger Predis detection */
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  tlib_pass_if_int_equal("library detected metric created", metric_count + 1,
                         nrm_table_size(NRPRG(txn)->unscoped_metrics));
  tlib_pass_if_not_null("library detected metric created",
                        nrm_find(NRPRG(txn)->unscoped_metrics,
                                 "Supportability/library/Predis/detected"));
  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_not_null("Predis\\Client::__construct wraprec created", wr);

  execute_count = NRTXNGLOBAL(execute_count);

  tlib_php_request_eval("new Predis\\Client();");

  // Because agent is recording, nr_php_instrument_func_begin and 
  // nr_php_instrument_func_end should execute.
  tlib_pass_if_size_t_equal(
      "when recording, func_begin/func_end should execute", execute_count + 1,
      NRTXNGLOBAL(execute_count));

  // Because agent is recording, wraprec instrumentation should be invoked.
  // **Note** : Predis instrumentation side effect is package version metric
  // creation and that fact is used to test if instrumentation was invoked.
  package = nr_php_packages_get_package(
      NRPRG(txn)->php_package_major_version_metrics_suggestions,
      "predis/predis");
  tlib_pass_if_not_null(
      "when recording, package version metric created", package);

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}
#endif

void test_main(void* p NRUNUSED) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8.0+ */
  test_register_handlers_globally_disabled();
  test_register_handlers_enabled_app_unknown();
  test_register_handlers_enabled_txn_ignored();
  test_register_handlers_enabled_and_recording();
#endif
}
