/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_txn_private.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static const nrtxnopt_t default_options = {
    .custom_events_enabled = 1,
    .synthetics_enabled = 0,
    .instance_reporting_enabled = 0,
    .database_name_reporting_enabled = 0,
    .err_enabled = 0,
    .request_params_enabled = 0,
    .autorum_enabled = 0,
    .analytics_events_enabled = 0,
    .error_events_enabled = 0,
    .tt_enabled = 0,
    .ep_enabled = 0,
    .tt_recordsql = 0,
    .tt_slowsql = 0,
    .apdex_t = 0,
    .tt_threshold = 0,
    .tt_is_apdex_f = 0,
    .ep_threshold = 0,
    .ss_threshold = 0,
    .cross_process_enabled = 0,
    .allow_raw_exception_messages = 0,
    .custom_parameters_enabled = 0,
};

static void test_is_policy_secure_null() {
  bool result;
  nrtxnopt_t options = default_options;

  result = nr_php_txn_is_policy_secure(NULL, NULL);
  tlib_pass_if_false(
      "Did two null values to nr_php_txn_is_policy_secure return false? ",
      result, "ERROR: expected result=false, actual result=%d", result);

  result = nr_php_txn_is_policy_secure(NULL, &options);
  tlib_pass_if_false(
      "Did a NULL policy and a valid options to nr_php_txn_is_policy_secure "
      "return false? ",
      result, "ERROR: expected result=false, actual result=%d", result);

  result = nr_php_txn_is_policy_secure("record_sql", NULL);
  tlib_pass_if_false(
      "Did a string policy and a NULL options to nr_php_txn_is_policy_secure "
      "return false? ",
      result, "ERROR: expected result=false, actual result=%d", result);

  result = nr_php_txn_is_policy_secure("unknown_policy", &options);
  tlib_pass_if_false(
      "Did an unknown policy and legit options to nr_php_txn_is_policy_secure "
      "return false? ",
      result, "ERROR: expected result=false, actual result=%d", result);
}

static void test_is_policy_secure_record_sql() {
  bool result;
  nrtxnopt_t options = default_options;

  options.tt_recordsql = NR_SQL_NONE;
  result = nr_php_txn_is_policy_secure("record_sql", &options);
  tlib_pass_if_true(
      "Is record_sql secure if options.tt_recordsql = NR_SQL_NONE? ", result,
      "ERROR: expected result=true, actual result=%d", result);

  options.tt_recordsql = NR_SQL_RAW;
  result = nr_php_txn_is_policy_secure("record_sql", &options);
  tlib_pass_if_false(
      "Is record_sql secure if options.tt_recordsql = NR_SQL_RAW? ", result,
      "ERROR: expected result=false, actual result=%d", result);

  options.tt_recordsql = NR_SQL_OBFUSCATED;
  result = nr_php_txn_is_policy_secure("record_sql", &options);
  tlib_pass_if_false(
      "Is record_sql secure if options.tt_recordsql = NR_SQL_NONE? ", result,
      "ERROR: expected result=false, actual result=%d", result);
}

static void test_is_policy_secure_allow_raw_exception_messages() {
  bool result;
  nrtxnopt_t options = default_options;

  options.allow_raw_exception_messages = 0;
  result
      = nr_php_txn_is_policy_secure("allow_raw_exception_messages", &options);
  tlib_pass_if_true(
      "Is allow_raw_exception_messages secure if "
      "options.allow_raw_exception_messages = 0? ",
      result, "ERROR: expected result=true, actual result=%d", result);

  options.allow_raw_exception_messages = 1;
  result
      = nr_php_txn_is_policy_secure("allow_raw_exception_messages", &options);
  tlib_pass_if_false(
      "Is allow_raw_exception_messages secure if "
      "options.allow_raw_exception_messages = 1? ",
      result, "ERROR: expected result=false, actual result=%d", result);
}

static void test_is_policy_secure_custom_parameters_enabled() {
  bool result;
  nrtxnopt_t options = default_options;

  options.custom_parameters_enabled = 0;
  result = nr_php_txn_is_policy_secure("custom_parameters", &options);
  tlib_pass_if_true(
      "Is custom_parameters secure if "
      "options.custom_parameters_enabled = 0? ",
      result, "ERROR: expected result=true, actual result=%d", result);

  options.custom_parameters_enabled = 1;
  result = nr_php_txn_is_policy_secure("custom_parameters", &options);
  tlib_pass_if_false(
      "Is custom_parameters secure if "
      "options.custom_parameters_enabled = 1? ",
      result, "ERROR: expected result=false, actual result=%d", result);
}

static void test_is_policy_secure_custom_events_enabled() {
  bool result;
  nrtxnopt_t options = default_options;

  options.custom_events_enabled = 0;
  result = nr_php_txn_is_policy_secure("custom_events", &options);
  tlib_pass_if_true(
      "Is custom_events secure if "
      "options.custom_events_enabled = 0? ",
      result, "ERROR: expected result=true, actual result=%d", result);

  options.custom_events_enabled = 1;
  result = nr_php_txn_is_policy_secure("custom_events", &options);
  tlib_pass_if_false(
      "Is custom_events secure if "
      "options.custom_events_enabled = 1? ",
      result, "ERROR: expected result=false, actual result=%d", result);
}

void test_main(void* p NRUNUSED) {
  /*
   * Four tests that ensure nr_php_txn_is_policy_secure works
   * and returns expected values when passed a test fixture,
   * and a fifth test for the null cases.
   */
  test_is_policy_secure_record_sql();
  test_is_policy_secure_allow_raw_exception_messages();
  test_is_policy_secure_custom_parameters_enabled();
  test_is_policy_secure_custom_events_enabled();
  test_is_policy_secure_null();
}
