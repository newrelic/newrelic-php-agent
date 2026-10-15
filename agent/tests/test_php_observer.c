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
#include "php_wrapper.h"
#include "nr_php_packages.h"

/*
 * PHP 8.5 always loads OPcache, so every engine these tests create maps
 * OPcache's shared memory. After several engines in one process, valgrind
 * loses track of the address space and crashes (seen with valgrind 3.25.1 on
 * amd64 musl). These tests don't need OPcache, which isn't loaded on PHP 8.4
 * and older, so turn it off.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_5_X_API_NO
#define OBSERVER_TEST_INI "opcache.enable=0\n"
#else
#define OBSERVER_TEST_INI ""
#endif

/*
 * Baseline: agent is disabled at the process level:
 *  - Zend Observer fcall_begin/fcall_end handlers do not get installed
 *  - Agent does not attempt to detect libraries
 *  - Agent does not create any wraprecs
 *  - Agent does not execute any special instrumentation
 */
static void test_register_handlers_globally_disabled(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;
  size_t execute_count = 0;

  tlib_php_engine_create(OBSERVER_TEST_INI "newrelic.enabled=false");
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_false("Agent globally disabled", NR_PHP_PROCESS_GLOBALS(enabled),
                     "Expected agent to be globally disabled");
  tlib_pass_if_null("NRPRG(txn) was not created", NRPRG(txn));

  // Trigger Predis detection
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  execute_count = NRTXNGLOBAL(execute_count);

  // Not much to test when agent is disabled but at least check that wraprec
  // was not created:
  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_null("Predis\\Client::__construct wraprec not created", wr);

  // Because agent is disabled per-request, observer handlers should not be
  // invoked. Thus nr_php_instrument_func_begin/nr_php_instrument_func_end
  // should not execute:
  tlib_pass_if_size_t_equal(
      "when disabled per-request, func_begin/func_end should not execute",
      execute_count, NRTXNGLOBAL(execute_count));

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

/*
 * When the agent is globally enabled but disabled for the current PHP request,
 * here emulated by explicitly setting NRINI(enabled) to 0, the agent should
 * behave as if it was globally disabled:
 *  - Zend Observer fcall_begin/fcall_end handlers do not get installed
 *  - Agent does not attempt to detect libraries
 *  - Agent does not create any wraprecs
 *  - Agent does not execute any special instrumentation
 */
static void test_register_handlers_per_request_disabled(void) {
  nruserfn_t* wr = NULL;
  zend_string* scope_name = NULL;
  zend_string* method_name = NULL;
  size_t execute_count = 0;

  tlib_php_engine_create(OBSERVER_TEST_INI);
  // Emulate disabling agent per-request by explicitly setting NRINI(enabled) to 0.
  NRINI(enabled) = 0;
  tlib_php_request_start();

  scope_name = zend_string_init(NR_PSTR("Predis\\Client"), 0);
  method_name = zend_string_init(NR_PSTR("__construct"), 0);

  tlib_pass_if_true("Agent globally enabled", NR_PHP_PROCESS_GLOBALS(enabled),
                    "Expected agent to be globally enabled");
  tlib_pass_if_null("NRPRG(txn) was not created", NRPRG(txn));

  // Trigger library detection
  tlib_php_request_eval("require '" PHP_SCRIPTS_DIR "/predis/src/Client.php';");

  wr = nr_php_user_instrument_wraprec_hashmap_get(method_name, scope_name);
  tlib_pass_if_null("Predis\\Client::__construct wraprec not created", wr);

  execute_count = NRTXNGLOBAL(execute_count);

  // Call auto-instrumented function:
  tlib_php_request_eval("new Predis\\Client();");

  // Because agent is disabled per-request, observer handlers should not be
  // invoked. Thus nr_php_instrument_func_begin/nr_php_instrument_func_end
  // should not execute:
  tlib_pass_if_size_t_equal(
      "when disabled per-request, func_begin/func_end should not execute",
      execute_count, NRTXNGLOBAL(execute_count));

  zend_string_free(scope_name);
  zend_string_free(method_name);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}

static nr_status_t mock_cmd_appinfo_unknown(nrapp_t* app) {
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

  tlib_php_engine_create(OBSERVER_TEST_INI);
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
  tlib_pass_if_true("Transaction is not being recorded", !nr_php_recording(),
                    "Expected transaction to be ignored");

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

  tlib_php_engine_create(OBSERVER_TEST_INI);
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

  tlib_php_engine_create(OBSERVER_TEST_INI);
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

/*
 * Handler override contract: a wraprec's custom fcall_handlers replace the
 * default Observer handlers only when both are set. The default begin and end
 * handlers work as a pair (stack depth, segment), so they are never mixed with
 * custom ones.
 */
static int custom_begin_calls = 0;
static int custom_end_calls = 0;

static void custom_fcall_begin(zend_execute_data* execute_data NRUNUSED) {
  custom_begin_calls += 1;
}

static void custom_fcall_end(zend_execute_data* execute_data NRUNUSED,
                             zval* return_value NRUNUSED) {
  custom_end_calls += 1;
}

typedef struct {
  int custom_begin_calls;
  int custom_end_calls;
  int default_begin_calls;
  int stack_depth_delta;
} override_result_t;

/*
 * Wraps o_target with the given custom handlers (optionally after calling it
 * once), calls it, and reports which handlers ran.
 */
static override_result_t call_with_custom_handlers(
    zend_observer_fcall_begin_handler begin,
    zend_observer_fcall_end_handler end,
    bool warm_up) {
  override_result_t result = {0};
  nruserfn_t* wr = NULL;
  int execute_count = 0;
  int depth = 0;

  tlib_php_engine_create(OBSERVER_TEST_INI);
  tlib_php_request_start();

  tlib_php_request_eval("function o_target() { return 1; }");
  if (warm_up) {
    tlib_php_request_eval("o_target();");
  }

  wr = nr_php_wrap_callable(nr_php_find_function("o_target"), NULL);
  tlib_pass_if_not_null("o_target wrapped", wr);
  if (NULL != wr) {
    wr->fcall_handlers.begin = begin;
    wr->fcall_handlers.end = end;
  }

  custom_begin_calls = 0;
  custom_end_calls = 0;
  execute_count = NRTXNGLOBAL(execute_count);
  depth = NRPRG_CTX(php_cur_stack_depth);

  tlib_php_request_eval("o_target();");

  result.custom_begin_calls = custom_begin_calls;
  result.custom_end_calls = custom_end_calls;
  result.default_begin_calls = NRTXNGLOBAL(execute_count) - execute_count;
  result.stack_depth_delta = NRPRG_CTX(php_cur_stack_depth) - depth;

  tlib_php_request_end();
  tlib_php_engine_destroy();
  return result;
}

static void test_override_both_handlers(void) {
  override_result_t r
      = call_with_custom_handlers(custom_fcall_begin, custom_fcall_end, false);

  tlib_pass_if_int_equal("both custom handlers set: custom begin ran", 1,
                         r.custom_begin_calls);
  tlib_pass_if_int_equal("both custom handlers set: custom end ran", 1,
                         r.custom_end_calls);
  tlib_pass_if_int_equal("both custom handlers set: default begin did not run",
                         0, r.default_begin_calls);
  tlib_pass_if_int_equal("both custom handlers set: stack depth balanced", 0,
                         r.stack_depth_delta);
}

static void test_override_begin_only_uses_defaults(void) {
  override_result_t r
      = call_with_custom_handlers(custom_fcall_begin, NULL, false);

  tlib_pass_if_int_equal("only custom begin set: custom begin did not run", 0,
                         r.custom_begin_calls);
  tlib_pass_if_int_equal("only custom begin set: default begin ran", 1,
                         r.default_begin_calls);
  tlib_pass_if_int_equal("only custom begin set: stack depth balanced", 0,
                         r.stack_depth_delta);
}

static void test_override_end_only_uses_defaults(void) {
  override_result_t r
      = call_with_custom_handlers(NULL, custom_fcall_end, false);

  tlib_pass_if_int_equal("only custom end set: custom end did not run", 0,
                         r.custom_end_calls);
  tlib_pass_if_int_equal("only custom end set: default begin ran", 1,
                         r.default_begin_calls);
  tlib_pass_if_int_equal("only custom end set: stack depth balanced", 0,
                         r.stack_depth_delta);
}

/*
 * Zend installs Observer handlers once, on a function's first call, into its
 * runtime cache. Custom handlers set after that are never used.
 */
static void test_override_after_first_call_has_no_effect(void) {
  override_result_t r
      = call_with_custom_handlers(custom_fcall_begin, custom_fcall_end, true);

  tlib_pass_if_int_equal(
      "custom handlers set after first call: custom begin did not run", 0,
      r.custom_begin_calls);
  tlib_pass_if_int_equal(
      "custom handlers set after first call: custom end did not run", 0,
      r.custom_end_calls);
  tlib_pass_if_int_equal(
      "custom handlers set after first call: default begin ran", 1,
      r.default_begin_calls);
  tlib_pass_if_int_equal(
      "custom handlers set after first call: stack depth balanced", 0,
      r.stack_depth_delta);
}

/*
 * A wraprec in the op_array extension slot that fails the magic check is not
 * used: its custom handlers are ignored and the default handlers are used. The
 * slot is seeded before the function's first call, the same way
 * nr_php_wraprec_lookup_set stores a wraprec.
 */
static void test_override_invalid_wraprec_uses_defaults(void) {
  nruserfn_t invalid_wr = {0}; /* magic is 0, not NR_USERFN_T_MAGIC */
  zend_function* zf = NULL;
  int execute_count = 0;
  int depth = 0;

  tlib_php_engine_create(OBSERVER_TEST_INI);
  tlib_php_request_start();

  tlib_php_request_eval("function o_target() { return 1; }");
  zf = nr_php_find_function("o_target");
  tlib_pass_if_not_null("o_target found", zf);
  if (NULL != zf) {
    invalid_wr.fcall_handlers.begin = custom_fcall_begin;
    invalid_wr.fcall_handlers.end = custom_fcall_end;
    zend_init_func_run_time_cache(&zf->op_array);
    ZEND_OP_ARRAY_EXTENSION(&zf->op_array,
                            NR_PHP_PROCESS_GLOBALS(op_array_extension_handle))
        = &invalid_wr;
  }

  custom_begin_calls = 0;
  custom_end_calls = 0;
  execute_count = NRTXNGLOBAL(execute_count);
  depth = NRPRG_CTX(php_cur_stack_depth);

  tlib_php_request_eval("o_target();");

  tlib_pass_if_int_equal("invalid wraprec: custom begin did not run", 0,
                         custom_begin_calls);
  tlib_pass_if_int_equal("invalid wraprec: custom end did not run", 0,
                         custom_end_calls);
  tlib_pass_if_int_equal("invalid wraprec: default begin ran", 1,
                         NRTXNGLOBAL(execute_count) - execute_count);
  tlib_pass_if_int_equal("invalid wraprec: stack depth balanced", 0,
                         NRPRG_CTX(php_cur_stack_depth) - depth);

  tlib_php_request_end();
  tlib_php_engine_destroy();
}
#endif

void test_main(void* p NRUNUSED) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8.0+ */
  test_register_handlers_globally_disabled();
  test_register_handlers_per_request_disabled();
  test_register_handlers_enabled_app_unknown();
  test_register_handlers_enabled_txn_ignored();
  test_register_handlers_enabled_and_recording();
  test_override_both_handlers();
  test_override_begin_only_uses_defaults();
  test_override_end_only_uses_defaults();
  test_override_after_first_call_has_no_effect();
  test_override_invalid_wraprec_uses_defaults();
#endif
}
