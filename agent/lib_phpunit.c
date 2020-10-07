/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_strings.h"

/*
 * PHPUnit instrumentation
 * =======================
 * This code instruments PHPUnit, a popular PHP unit test framework. Test suite
 * summary data are sent as custom "TestSuite" events. Individual test cases run
 * by each suite send their data as custom "Test" events. Test cases are tied to
 * their parent suite by a unique run id.
 *
 * We won't send events if the function arguments aren't found. If we can't get
 * data for a field, we send an empty string or NULL and log a message.
 *
 * Source : https://github.com/sebastianbergmann/phpunit
 * Docs   : https://phpunit.de/manual/current/en/index.html
 */

typedef struct _nr_phpunit_test_status_t {
  const char* status;
  const char* display_name;
} nr_phpunit_test_status_t;

static const nr_phpunit_test_status_t nr_phpunit_test_statuses[] = {
    {"STATUS_PASSED", "passed"},         {"STATUS_SKIPPED", "skipped"},
    {"STATUS_INCOMPLETE", "incomplete"}, {"STATUS_FAILURE", "failed"},
    {"STATUS_ERROR", "error"},           {"STATUS_RISKY", "risky"},
    {"STATUS_WARNING", "warning"},
};

static const size_t num_statuses
    = sizeof(nr_phpunit_test_statuses) / sizeof(nr_phpunit_test_status_t);

typedef struct _nr_phpunit_test_event_fields_t {
  const char* name;
  const char* suite;
  const char* outcome;
  zend_long num_assertions;
  double duration_secs;
  const char* message;
} nr_phpunit_test_event_fields_t;

/*
 * Purpose: Encapsulates logic for "is this zval a PHP object" and "is that
 *          object an instance of a PHPUnit Test Suite"
 *
 * Params:  The zval we're evaluating
 *
 * Returns: true if the object is a class of the expected type, false otherwise
 */
static bool nr_phpunit_is_zval_a_testsuite(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
             obj, "PHPUnit\\Framework\\TestSuite" TSRMLS_CC)
         || nr_php_object_instanceof_class(
                obj, "PHPUnit_Framework_TestSuite" TSRMLS_CC);
}

/*
 * Purpose: Encapsulates logic for "is this zval a PHP object" and "is that
 *          object an instance of a PHPUnit Test Result"
 *
 * Params:  The zval we're evaluating
 *
 * Returns: true if the object is a class of the expected type, false otherwise
 */
static bool nr_phpunit_is_zval_a_testresult(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
             obj, "PHPUnit\\Framework\\TestResult" TSRMLS_CC)
         || nr_php_object_instanceof_class(
                obj, "PHPUnit_Framework_TestResult" TSRMLS_CC);
}

/*
 * Purpose: Encapsulates logic for "is this zval a PHP object" and "is that
 *          object an instance of a PHPUnit Test Failure"
 *
 * Params:  The zval we're evaluating
 *
 * Returns: true if the object is a class of the expected type, false otherwise
 */
static bool nr_phpunit_is_zval_a_testfailure(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
             obj, "PHPUnit\\Framework\\TestFailure" TSRMLS_CC)
         || nr_php_object_instanceof_class(
                obj, "PHPUnit_Framework_TestFailure" TSRMLS_CC);
}

/*
 * Purpose: Encapsulates logic for "is this zval a PHP object" and "is that
 *          object an instance of a PHPUnit Test Case"
 *
 * Params:  The zval we're evaluating
 *
 * Returns: true if the object is a class of the expected type, false otherwise
 */
static bool nr_phpunit_is_zval_a_testcase(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(obj,
                                        "PHPUnit_Framework_TestCase" TSRMLS_CC)
         || nr_php_object_instanceof_class(
                obj, "PHPUnit\\Framework\\TestCase" TSRMLS_CC);
}

/*
 * Purpose: Encapsulates logic for "is this zval a PHP object" and "is that
 *          object an instance of a PHPUnit Skipped Test"
 *
 * Params:  The zval we're evaluating
 *
 * Returns: true if the object is a class of the expected type, false otherwise
 */
static bool nr_phpunit_is_zval_a_skippedtest(zval* obj TSRMLS_DC) {
  return nr_php_object_instanceof_class(
             obj, "PHPUnit\\Framework\\SkippedTest" TSRMLS_CC)
         || nr_php_object_instanceof_class(
                obj, "PHPUnit_Framework_SkippedTest" TSRMLS_CC);
}

static char* nr_phpunit_get_suite_name(zval* result TSRMLS_DC) {
  zval* suite = NULL;
  char* name = NULL;
  zval* name_zv = NULL;

  suite = nr_php_call(result, "topTestSuite");
  if (!nr_phpunit_is_zval_a_testsuite(suite TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test suite",
                     __func__);
    goto leave;
  }

  name_zv = nr_php_call(suite, "getName");
  if (nr_php_is_zval_valid_string(name_zv)) {
    name = nr_strndup(Z_STRVAL_P(name_zv), Z_STRLEN_P(name_zv));
  }

leave:
  nr_php_zval_free(&name_zv);
  nr_php_zval_free(&suite);
  return name;
}

/*
 * The active transaction's guid seemed like a good candidate for a unique
 * identifier to link individual test events to their suite event.
 */
static const char* nr_phpunit_get_unique_identifier(TSRMLS_D) {
  if (NULL == NRPRG(txn)) {
    return NULL;
  }
  return nr_txn_get_guid(NRPRG(txn));
}

static int nr_phpunit_was_test_successful(zval* result TSRMLS_DC) {
  zval* successful_zv = NULL;
  int successful = 0;

  successful_zv = nr_php_call(result, "wasSuccessful");
  if (!nr_php_is_zval_valid_bool(successful_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: unable to determine whether suite was successful",
                     __func__);
    goto leave;
  }

  successful = nr_php_is_zval_true(successful_zv);

leave:
  nr_php_zval_free(&successful_zv);
  return successful;
}

static zend_long nr_phpunit_get_count(zval* result,
                                      const char* method_name TSRMLS_DC) {
  zval* value_zv = NULL;
  zend_long count = 0;

  value_zv = nr_php_call(result, method_name);
  if (0 == nr_php_is_zval_valid_integer(value_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: method call to \"%s\" did not return a long",
                     __func__, method_name);
    return count;
  }

  count = Z_LVAL_P(value_zv);
  nr_php_zval_free(&value_zv);

  return count;
}

static size_t nr_phpunit_get_passed_count(zval* result TSRMLS_DC) {
  zval* passed_zv = NULL;
  size_t passed = 0;

  passed_zv = nr_php_call(result, "passed");
  if (!nr_php_is_zval_valid_array(passed_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: unable to obtain number of passed tests", __func__);
    goto leave;
  }

  passed = nr_php_zend_hash_num_elements(Z_ARRVAL_P(passed_zv));

leave:
  nr_php_zval_free(&passed_zv);
  return passed;
}

static zend_long nr_phpunit_get_num_assertions(zval* printer TSRMLS_DC) {
  zval* assertions_zv = NULL;

  assertions_zv
      = nr_php_get_zval_object_property(printer, "numAssertions" TSRMLS_CC);
  if (!nr_php_is_zval_valid_integer(assertions_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: unable to obtain number of test assertions",
                     __func__);
    return 0;
  }

  return Z_LVAL_P(assertions_zv);
}

static double nr_phpunit_get_duration(zval* result TSRMLS_DC) {
  zval* duration_zv = NULL;

  duration_zv = nr_php_get_zval_object_property(result, "time" TSRMLS_CC);
  if (!nr_php_is_zval_valid_double(duration_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test duration",
                     __func__);
    return 0;
  }

  return Z_DVAL_P(duration_zv);
}

/*
 * This function generates a test suite event for each suite run.
 */
NR_PHP_WRAPPER(nr_phpunit_instrument_resultprinter_printresult) {
  zval* result = NULL;
  zval* this_var = NULL;
  char* suite_name;
  nrobj_t* event;

  (void)wraprec;

  if (0 == NRINI(phpunit_events_enabled)) {
    NR_PHP_WRAPPER_LEAVE;
  }

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain scope", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  result = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_phpunit_is_zval_a_testresult(result TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test result",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  suite_name = nr_phpunit_get_suite_name(result TSRMLS_CC);

  event = nro_new_hash();
  nro_set_hash_string(event, "name", suite_name);
  nro_set_hash_string(event, "runId",
                      nr_phpunit_get_unique_identifier(TSRMLS_C));
  nro_set_hash_boolean(event, "successful",
                       nr_phpunit_was_test_successful(result TSRMLS_CC));
  nro_set_hash_long(event, "testCount",
                    nr_phpunit_get_count(result, "count" TSRMLS_CC));
  nro_set_hash_int(event, "passedCount",
                   nr_phpunit_get_passed_count(result TSRMLS_CC));
  nro_set_hash_long(event, "failedCount",
                    nr_phpunit_get_count(result, "failureCount" TSRMLS_CC));
  nro_set_hash_long(event, "skippedCount",
                    nr_phpunit_get_count(result, "skippedCount" TSRMLS_CC));
  nro_set_hash_long(event, "errorCount",
                    nr_phpunit_get_count(result, "errorCount" TSRMLS_CC));
  nro_set_hash_long(event, "riskyCount",
                    nr_phpunit_get_count(result, "riskyCount" TSRMLS_CC));
  nro_set_hash_long(
      event, "incompleteCount",
      nr_phpunit_get_count(result, "notImplementedCount" TSRMLS_CC));
  nro_set_hash_long(event, "warningCount",
                    nr_phpunit_get_count(result, "warningCount" TSRMLS_CC));
  nro_set_hash_long(event, "assertionCount",
                    nr_phpunit_get_num_assertions(this_var TSRMLS_CC));
  nro_set_hash_double(event, "duration",
                      nr_phpunit_get_duration(result TSRMLS_CC));

  nr_txn_record_custom_event(NRPRG(txn), "TestSuite", event);

end:
  nr_php_scope_release(&this_var);
  nr_php_arg_release(&result);
  nr_free(suite_name);
  nro_delete(event);
}
NR_PHP_WRAPPER_END

static int nr_phpunit_did_last_test_fail(zval* result TSRMLS_DC) {
  zval* last_test_failed = NULL;

  last_test_failed
      = nr_php_get_zval_object_property(result, "lastTestFailed" TSRMLS_CC);
  if (!nr_php_is_zval_valid_bool(last_test_failed)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: unable to determine whether last test failed",
                     __func__);
    return 0;
  }

  return nr_php_is_zval_true(last_test_failed);
}

static const char* nr_phpunit_determine_test_outcome(zval* this_var,
                                                     zval* test_case
                                                         TSRMLS_DC) {
  zval* outcome_zv = NULL;
  zend_long index;
  const char* outcome = NULL;

  outcome_zv = nr_php_call(test_case, "getStatus");
  if (!nr_php_is_zval_valid_integer(outcome_zv)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test outcome",
                     __func__);
    goto leave;
  }

  index = (zend_long)Z_LVAL_P(outcome_zv);
  if ((index >= 0) && ((size_t)index <= num_statuses - 1)) {
    outcome = nr_phpunit_test_statuses[index].display_name;
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unknown test status: %li", __func__,
                     (long)index);
    goto leave;
  }

  /*
   * The outcomes "risky" or "warning" are special snowflakes. Risky tests
   * aren't added to the passedCount and don't cause the suite to fail. Warning
   * tests ARE added to passedCount but they cause the suite to fail. Neither
   * are appropriately labeled as they pass through endTest so we detect them
   * like so:
   * Last test failed + "passed" = actually "risky"
   * Last test passed + "error" = actually "warning"
   */
  if (nr_phpunit_did_last_test_fail(this_var TSRMLS_CC)) {
    if (0 == nr_strcmp(outcome, "passed")) {
      outcome = "risky";
    }
  } else if (0 == nr_strcmp(outcome, "error")) {
    outcome = "warning";
  }

leave:
  nr_php_zval_free(&outcome_zv);
  return outcome;
}

static void nr_phpunit_send_test_event(
    const nr_phpunit_test_event_fields_t* fields TSRMLS_DC) {
  nrobj_t* event;

  event = nro_new_hash();
  nro_set_hash_string(event, "name", fields->name);
  nro_set_hash_string(event, "testSuiteName", fields->suite);
  nro_set_hash_string(event, "runId",
                      nr_phpunit_get_unique_identifier(TSRMLS_C));
  nro_set_hash_string(event, "outcome", fields->outcome);
  nro_set_hash_long(event, "assertionCount", fields->num_assertions);
  nro_set_hash_double(event, "duration", fields->duration_secs);
  nro_set_hash_string(event, "message", fields->message);

  nr_txn_record_custom_event(NRPRG(txn), "Test", event);

  nro_delete(event);
}

/*
 * TestFailures are created for exceptions and stored in arrays on the TestCase
 * instance. Messages for risky and warning tests are sourced from here.
 */
static char* nr_phpunit_get_message_for_test(zval* result,
                                             const char* test_type TSRMLS_DC) {
  zval* tests = NULL;
  size_t num_tests = 0;
  zval* failure_zv = NULL;
  char* message = NULL;
  zval* message_zv = NULL;

  tests = nr_php_call(result, test_type);
  if (!nr_php_is_zval_valid_array(tests)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain tests", __func__);
    goto leave;
  }

  num_tests = nr_php_zend_hash_num_elements(Z_ARRVAL_P(tests));
  if (0 == num_tests) {
    goto leave;
  }

  failure_zv = nr_php_zend_hash_index_find(Z_ARRVAL_P(tests), num_tests - 1);
  if (!nr_phpunit_is_zval_a_testfailure(failure_zv TSRMLS_CC)) {
    goto leave;
  }

  message_zv = nr_php_call(failure_zv, "getExceptionAsString");
  if (nr_php_is_zval_valid_string(message_zv)) {
    message = nr_strndup(Z_STRVAL_P(message_zv), Z_STRLEN_P(message_zv));
  }

leave:
  nr_php_zval_free(&message_zv);
  nr_php_zval_free(&tests);
  return message;
}

/*
 * This function generates a test event for each completed test run.
 */
NR_PHP_WRAPPER(nr_phpunit_instrument_testresult_endtest) {
  zval* test_case = NULL;
  zval* this_var = NULL;
  zval* duration = NULL;
  zval* test_case_status = NULL;
  const char* outcome;
  char* name = NULL;
  zval* name_zv = NULL;
  char* suite = NULL;
  char* message = NULL;
  nr_phpunit_test_event_fields_t fields;

  (void)wraprec;

  if (0 == NRINI(phpunit_events_enabled)) {
    NR_PHP_WRAPPER_LEAVE;
  }

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain scope", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  test_case = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_phpunit_is_zval_a_testcase(test_case TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test case",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  /*
   * PHPUnit 6+ started passing "tests skipped due to dependency failures"
   * to the endTest method -- however, we already catch these tests in
   * our nr_phpunit_instrument_testresult_adderror wrapper.  This check
   * ensures these skipped tests aren't double counted by bailing if
   * a test's status isn't set.
   */
  test_case_status = nr_php_call(test_case, "getStatus");
  if (nr_php_is_zval_null(test_case_status)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: null test case status, treating as skipped",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  duration = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_double(duration)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: invalid test duration", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  outcome = nr_phpunit_determine_test_outcome(this_var, test_case TSRMLS_CC);
  name_zv = nr_php_call(test_case, "getName");
  if (nr_php_is_zval_valid_string(name_zv)) {
    name = nr_strndup(Z_STRVAL_P(name_zv), Z_STRLEN_P(name_zv));
  }
  suite = nr_phpunit_get_suite_name(this_var TSRMLS_CC);

  /*
   * Risky test messages are stored in their exception and need to be accessed
   * differently.
   */
  if (0 == nr_strcmp(outcome, "risky")) {
    message = nr_phpunit_get_message_for_test(this_var, "risky" TSRMLS_CC);
  } else {
    zval* message_zv = nr_php_call(test_case, "getStatusMessage");

    if (nr_php_is_zval_valid_string(message_zv)) {
      message = nr_strndup(Z_STRVAL_P(message_zv), Z_STRLEN_P(message_zv));
    }

    nr_php_zval_free(&message_zv);
  }

  nr_memset(&fields, 0, sizeof(fields));
  fields.name = name;
  fields.suite = suite;
  fields.outcome = outcome;
  fields.num_assertions
      = nr_phpunit_get_count(test_case, "getNumAssertions" TSRMLS_CC);
  fields.duration_secs = Z_DVAL_P(duration);
  fields.message = message;

  nr_phpunit_send_test_event(&fields TSRMLS_CC);

end:
  nr_php_scope_release(&this_var);
  nr_php_arg_release(&test_case);
  nr_php_arg_release(&duration);
  nr_free(name);
  nr_free(suite);
  nr_free(message);
  nr_php_zval_free(&name_zv);
  nr_php_zval_free(&test_case_status);
}
NR_PHP_WRAPPER_END

/*
 * This function catches tests that PHPUnit marks as "skipped" due to failing
 * dependencies. Since they are never actually run, they do not go through the
 * endTest code path like other skipped tests.
 */
NR_PHP_WRAPPER(nr_phpunit_instrument_testresult_adderror) {
  zval* test_case = NULL;
  zval* exception = NULL;
  zval* this_var = NULL;
  zval* name_zv = NULL;
  char* name = NULL;
  char* suite = NULL;
  char* message = NULL;
  nr_phpunit_test_event_fields_t fields;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  if (!NRINI(phpunit_events_enabled)) {
    NR_PHP_WRAPPER_LEAVE;
  }

  exception = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_phpunit_is_zval_a_skippedtest(exception TSRMLS_CC)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain scope", __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  test_case = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_phpunit_is_zval_a_testcase(test_case TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: unable to obtain test case",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  name_zv = nr_php_call(test_case, "getName");
  if (nr_php_is_zval_valid_string(name_zv)) {
    name = nr_strndup(Z_STRVAL_P(name_zv), Z_STRLEN_P(name_zv));
  }
  suite = nr_phpunit_get_suite_name(this_var TSRMLS_CC);

  /*
   * PHPUnit 3.7 doesn't have an Exception class, so we can't access the message
   * directly from the exception. Instead we'll check the last skipped test.
   */
  if (nr_phpunit_did_last_test_fail(this_var TSRMLS_CC)) {
    message = nr_phpunit_get_message_for_test(this_var, "skipped" TSRMLS_CC);
  }

  nr_memset(&fields, 0, sizeof(fields));
  fields.name = name;
  fields.suite = suite;
  fields.outcome = "skipped";
  fields.num_assertions = 0;
  fields.duration_secs = 0;
  fields.message = message;

  nr_phpunit_send_test_event(&fields TSRMLS_CC);

end:
  nr_php_scope_release(&this_var);
  nr_php_arg_release(&test_case);
  nr_php_arg_release(&exception);
  nr_php_zval_free(&name_zv);
  nr_free(name);
  nr_free(suite);
  nr_free(message);
}
NR_PHP_WRAPPER_END

/*
 * Sanity check our hard-coded table of test status codes. We look up each
 * status and verify that its code matches our expectations. This allows us to
 * quickly reference them when evaluating a test outcome.
 */
static int nr_phpunit_are_statuses_valid(TSRMLS_D) {
  zend_class_entry* class_entry
      = nr_php_find_class("phpunit_runner_basetestrunner" TSRMLS_CC);

  // if we can't find the underscore/fake-namespace version, look
  // for the real namespaced version
  if (NULL == class_entry) {
    class_entry
        = nr_php_find_class("phpunit\\runner\\basetestrunner" TSRMLS_CC);
  }

  if (NULL == class_entry) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "could not find PHPUnit_Runner_BaseTestRunner or "
                     "PHPUnit\\Runner\\BaseTestRunner ");
    return 0;
  }

  /* The first five statuses are present for PHPUnit 3.0+ */
  for (size_t i = 0; i < 5; i++) {
    const char* status = nr_phpunit_test_statuses[i].status;
    zval* constant = NULL;

    constant = nr_php_get_class_constant(class_entry, status);
    if ((!nr_php_is_zval_valid_integer(constant))
        || (Z_LVAL_P(constant) != (int)i)) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "%s: %s constant has an unexpected value", __func__,
                       status);
      nr_php_zval_free(&constant);
      return 0;
    }
    nr_php_zval_free(&constant);
  }

  return 1;
}

void nr_phpunit_enable(TSRMLS_D) {
  if (!NRINI(phpunit_events_enabled)) {
    return;
  }

  if (!nr_phpunit_are_statuses_valid(TSRMLS_C)) {
    return;
  }

  nr_php_wrap_user_function(
      NR_PSTR("PHPUnit_TextUI_ResultPrinter::printResult"),
      nr_phpunit_instrument_resultprinter_printresult TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("PHPUnit\\TextUI\\ResultPrinter::printResult"),
      nr_phpunit_instrument_resultprinter_printresult TSRMLS_CC);

  nr_php_wrap_user_function(NR_PSTR("PHPUnit_Framework_TestResult::endTest"),
                            nr_phpunit_instrument_testresult_endtest TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("PHPUnit\\Framework\\TestResult::endTest"),
                            nr_phpunit_instrument_testresult_endtest TSRMLS_CC);

  nr_php_wrap_user_function(
      NR_PSTR("PHPUnit_Framework_TestResult::addError"),
      nr_phpunit_instrument_testresult_adderror TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("PHPUnit\\Framework\\TestResult::addError"),
      nr_phpunit_instrument_testresult_adderror TSRMLS_CC);
}
