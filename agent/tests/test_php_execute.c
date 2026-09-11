/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"

#include "nr_commands.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_execute.h"
#include "php_execute_private.h"
#include "php_globals.h"
#include "php_wrapper.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

NR_PHP_WRAPPER(test_add_metric_in_wrapper) {
  (void)wraprec;

  nr_segment_add_metric(auto_segment, "metric", true);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

static void test_add_segment_metric(TSRMLS_D) {
  size_t segment_count;
  zval* expr;

  tlib_php_request_start();

  /*
   * Setting this value very high, so segments aren't created on
   * slow machines.
   */
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 1000000;

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval("function f2() { return 4; }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("f2"),
                            test_add_metric_in_wrapper TSRMLS_CC);

  segment_count = NRPRG(txn)->segment_count;

  /*
   * Call a short function with no segment metrics added. This should not
   * increase the segment count.
   */
  expr = nr_php_call(NULL, "f1");
  tlib_pass_if_size_t_equal("no segment created", segment_count,
                            NRPRG(txn)->segment_count);
  nr_php_zval_free(&expr);

  /*
   * Call a short function with segment metrics added. This should increase the
   * segment count.
   */
  expr = nr_php_call(NULL, "f2");
  tlib_pass_if_size_t_equal("segment created", segment_count + 1,
                            NRPRG(txn)->segment_count);
  nr_php_zval_free(&expr);

  tlib_php_request_end();
}

static void test_txn_restart_in_callstack(TSRMLS_D) {
  zval* expr;

  tlib_php_request_start();

  /*
   * Keep all the segments.
   */
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 0;

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval(
      "function f2() { "
      "newrelic_end_transaction(); "
      "newrelic_start_transaction(\"name\");"
      "}" TSRMLS_CC);
  tlib_php_request_eval("function f3() { f1(); f2(); }" TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("f1"),
                            test_add_metric_in_wrapper TSRMLS_CC);

  /*
   * This should create a regular segment and a metric for f1, which must get
   * properly cleaned up when the transaction is discarded.
   *
   * This test is primarily for testing that no memory is leaked in this
   * case.
   */
  expr = nr_php_call(NULL, "f3");
  nr_php_zval_free(&expr);

  tlib_php_request_end();
}

static void test_php_cur_stack_depth(TSRMLS_D) {
  zval* expr;

  tlib_php_request_start();

  tlib_php_request_eval("function f1() { return 4; }" TSRMLS_CC);
  tlib_php_request_eval(
      "function f2() { newrelic_ignore_transaction(); return 4; }" TSRMLS_CC);

  expr = nr_php_call(NULL, "f1");
  nr_php_zval_free(&expr);

  tlib_pass_if_int_equal("PHP stack depth tracking when recording", 0,
                         NRPRG_CTX(php_cur_stack_depth));

  expr = nr_php_call(NULL, "f2");
  nr_php_zval_free(&expr);

  tlib_pass_if_int_equal("PHP stack depth tracking when ignoring", 0,
                         NRPRG_CTX(php_cur_stack_depth));

  tlib_php_request_end();
}

static nr_status_t mock_cmd_appinfo_unknown(int daemon_fd NRUNUSED,
                                            nrapp_t* app) {
  app->state = NR_APP_UNKNOWN;
  return NR_SUCCESS;
}

/*
 * The gate this PR relaxed (nr_php_fcall_register_handlers now gates on
 * NR_PHP_PROCESS_GLOBALS(enabled) instead of nr_php_recording()) means
 * library/framework/logging-framework detection can now run for the first
 * time on a given op_array while NRPRG(txn) is NULL. The most direct way to
 * reach that state is the very first request in a process: RINIT calls
 * appinfo, and until the daemon confirms the app, NRPRG(txn) is never
 * created at all. mock_cmd_appinfo_unknown reproduces exactly that by
 * forcing app->state = NR_APP_UNKNOWN on every appinfo call.
 *
 * nr_fw_support_add_*_supportability_metric() are NULL-txn-safe, but every
 * enable() callback in these tables is third-party code we don't control
 * call-by-call; this test walks every table entry through the real
 * dispatcher to catch a future enable() callback that dereferences the txn
 * directly.
 *
 * nr_php_user_instrumentation_from_file (not nr_php_execute_file) is the
 * right target here: nr_php_execute_file also re-executes the file's real
 * op array via orig_execute, which a fabricated filename can't do safely.
 * The dispatcher just string-matches, so passing a table's file_to_check
 * value AS the filename trivially self-matches - no fixture files needed.
 *
 * The libraries/logging_frameworks/vuln_mgmt_packages tables are walked once
 * per all_frameworks[] entry, not once overall: in a real request, a
 * framework's file loads first and sets NRPRG_SHARED(current_framework) for
 * the rest of that request, and libraries are detected afterward within
 * that context. A library's enable() shouldn't depend on which framework is
 * active, but nothing stops one from starting to, so this mirrors the real
 * per-request sequence (each framework x the full library/logging/package
 * set) instead of testing library detection in isolation from framework
 * context.
 */
static void test_user_instrumentation_from_file_app_unknown() {
  tlib_php_engine_create("");

  // Emulate the very first request in a process: appinfo hasn't confirmed
  // the app yet, so RINIT never creates a transaction. This is a valid
  // state for the agent to be in.
  nr_cmd_appinfo_hook = mock_cmd_appinfo_unknown;

  for (int i = 0; i < num_all_frameworks; i++) {
    size_t ii;
    tlib_php_request_start();
    tlib_pass_if_null("NRPRG(txn) was not created", NRPRG(txn));
    tlib_pass_if_true("Transaction is not being recorded", !nr_php_recording(),
                      "Expected transaction to be ignored");
    nr_php_user_instrumentation_from_file(NR_PSTR("vendor/autoload.php"));
    nr_php_user_instrumentation_from_file(all_frameworks[i].file_to_check,
                                          all_frameworks[i].file_to_check_len);

    for (ii = 0; ii < num_libraries; ii++) {
      nr_php_user_instrumentation_from_file(libraries[ii].file_to_check,
                                            libraries[ii].file_to_check_len);
    }

    for (ii = 0; ii < num_logging_frameworks; ii++) {
      nr_php_user_instrumentation_from_file(
          logging_frameworks[ii].file_to_check,
          logging_frameworks[ii].file_to_check_len);
    }

    for (ii = 0; ii < num_packages; ii++) {
      nr_php_user_instrumentation_from_file(
          vuln_mgmt_packages[ii].file_to_check,
          vuln_mgmt_packages[ii].file_to_check_len);
    }

    tlib_php_request_end();
  }
  tlib_php_engine_destroy();
}

/*
 * Complementary to test_user_instrumentation_from_file_app_unknown above:
 * here NRPRG(txn) exists, but newrelic_ignore_transaction() has marked it
 * as not recording. This can't exercise the NULL-txn dereference risk that
 * test covers - the fw_support NULL guards only check for NULL, and an
 * ignored-but-alive txn sails through them - but it does confirm detection
 * dispatch still runs correctly, without crashing or leaking, while the
 * transaction isn't recording, which the same gate relaxation also newly
 * allows.
 */
static void test_user_instrumentation_from_file_txn_ignored() {
  tlib_php_engine_create("");

  for (int i = 0; i < num_all_frameworks; i++) {
    size_t ii;
    tlib_php_request_start();
    tlib_php_request_eval("newrelic_ignore_transaction();");
    tlib_pass_if_not_null("NRPRG(txn) was created", NRPRG(txn));
    tlib_pass_if_true("Transaction is not being recorded", !nr_php_recording(),
                      "Expected transaction to be ignored");

    nr_php_user_instrumentation_from_file(NR_PSTR("vendor/autoload.php"));
    nr_php_user_instrumentation_from_file(all_frameworks[i].file_to_check,
                                          all_frameworks[i].file_to_check_len);

    for (ii = 0; ii < num_libraries; ii++) {
      nr_php_user_instrumentation_from_file(libraries[ii].file_to_check,
                                            libraries[ii].file_to_check_len);
    }

    for (ii = 0; ii < num_logging_frameworks; ii++) {
      nr_php_user_instrumentation_from_file(
          logging_frameworks[ii].file_to_check,
          logging_frameworks[ii].file_to_check_len);
    }

    for (ii = 0; ii < num_packages; ii++) {
      nr_php_user_instrumentation_from_file(
          vuln_mgmt_packages[ii].file_to_check,
          vuln_mgmt_packages[ii].file_to_check_len);
    }

    tlib_php_request_end();
  }
  tlib_php_engine_destroy();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);
  test_add_segment_metric(TSRMLS_C);
  test_txn_restart_in_callstack(TSRMLS_C);
  test_php_cur_stack_depth(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
  test_user_instrumentation_from_file_app_unknown();
  test_user_instrumentation_from_file_txn_ignored();
}
