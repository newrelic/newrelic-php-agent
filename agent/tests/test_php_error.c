/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_main.h"

#include "nr_attributes.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_globals.h"
#include "php_error.h"
#include "php_newrelic.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_error_get_priority() {
  /*
   * Test : Unknown type.
   */
  tlib_pass_if_int_equal("Unknown error type", 20,
                         nr_php_error_get_priority(-1));
  tlib_pass_if_int_equal("Unknown error type", 20,
                         nr_php_error_get_priority(3));

  /*
   * Test : Normal operation.
   */

  tlib_pass_if_int_equal("Known error type", 0,
                         nr_php_error_get_priority(E_NOTICE));
  tlib_pass_if_int_equal("Known error type", 0,
                         nr_php_error_get_priority(E_USER_NOTICE));
  tlib_pass_if_int_equal("Known error type", 10,
                         nr_php_error_get_priority(E_STRICT));
  tlib_pass_if_int_equal("Known error type", 30,
                         nr_php_error_get_priority(E_USER_DEPRECATED));
  tlib_pass_if_int_equal("Known error type", 30,
                         nr_php_error_get_priority(E_DEPRECATED));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_USER_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_CORE_WARNING));
  tlib_pass_if_int_equal("Known error type", 40,
                         nr_php_error_get_priority(E_COMPILE_WARNING));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_RECOVERABLE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_USER_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_CORE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_COMPILE_ERROR));
  tlib_pass_if_int_equal("Known error type", 50,
                         nr_php_error_get_priority(E_PARSE));
}

static void test_get_error_type_string() {
  /*
   * Test : Unknown type.
   */
  tlib_pass_if_str_equal("Unknown error type", "Error",
                         nr_php_error_get_type_string(-1));
  tlib_pass_if_str_equal("Unknown error type", "Error",
                         nr_php_error_get_type_string(3));

  /*
   * Test : Normal operation.
   */

  tlib_pass_if_str_equal("Known error type", "E_NOTICE",
                         nr_php_error_get_type_string(E_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_USER_NOTICE",
                         nr_php_error_get_type_string(E_USER_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_STRICT",
                         nr_php_error_get_type_string(E_STRICT));
  tlib_pass_if_str_equal("Known error type", "E_USER_NOTICE",
                         nr_php_error_get_type_string(E_USER_NOTICE));
  tlib_pass_if_str_equal("Known error type", "E_USER_DEPRECATED",
                         nr_php_error_get_type_string(E_USER_DEPRECATED));
  tlib_pass_if_str_equal("Known error type", "E_DEPRECATED",
                         nr_php_error_get_type_string(E_DEPRECATED));
  tlib_pass_if_str_equal("Known error type", "E_USER_WARNING",
                         nr_php_error_get_type_string(E_USER_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_WARNING",
                         nr_php_error_get_type_string(E_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_CORE_WARNING",
                         nr_php_error_get_type_string(E_CORE_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_COMPILE_WARNING",
                         nr_php_error_get_type_string(E_COMPILE_WARNING));
  tlib_pass_if_str_equal("Known error type", "E_RECOVERABLE_ERROR",
                         nr_php_error_get_type_string(E_RECOVERABLE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_ERROR",
                         nr_php_error_get_type_string(E_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_USER_ERROR",
                         nr_php_error_get_type_string(E_USER_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_CORE_ERROR",
                         nr_php_error_get_type_string(E_CORE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_COMPILE_ERROR",
                         nr_php_error_get_type_string(E_COMPILE_ERROR));
  tlib_pass_if_str_equal("Known error type", "E_PARSE",
                         nr_php_error_get_type_string(E_PARSE));
}

/*
 * The error_group callback (nr_php_error_call_error_group_callback) is
 * a static helper invoked from nr_php_error_record_exception when the user
 * has registered a callback via newrelic_set_error_group_callback. The tests
 * below exercise it indirectly via nr_php_error_record_exception, observing
 * the resulting "error.group.name" agent attribute on the transaction.
 *
 * The callable zval used to populate fci/fcc is held in callable_holder for
 * the duration of the test so that fci.function_name remains valid until
 * after the call has been made.
 */
static zval* callable_holder;

static void install_error_group_callback(const char* callable_expr TSRMLS_DC) {
  zend_fcall_info fci;
  zend_fcall_info_cache fcc;

  nr_memset(&fci, 0, sizeof(fci));
  nr_memset(&fcc, 0, sizeof(fcc));

  callable_holder = tlib_php_request_eval_expr(callable_expr TSRMLS_CC);
  tlib_pass_if_not_null("install_error_group_callback: callable parsed",
                        callable_holder);

  tlib_pass_if_int_equal(
      "install_error_group_callback: zend_fcall_info_init succeeds", SUCCESS,
      zend_fcall_info_init(callable_holder, 0, &fci, &fcc, NULL, NULL));

  NRPRG_SHARED(error_group_user_callback).fci = fci;
  NRPRG_SHARED(error_group_user_callback).fcc = fcc;
  NRPRG_SHARED(error_group_user_callback).is_set = true;
}

static void uninstall_error_group_callback(void) {
  NRPRG_SHARED(error_group_user_callback).is_set = false;
  if (NULL != callable_holder) {
    nr_php_zval_free(&callable_holder);
  }
}

static char* lookup_error_group_name(nrtxn_t* txn) {
  nrobj_t* attrs;
  const char* val;
  char* result = NULL;

  attrs = nr_attributes_agent_to_obj(txn->attributes,
                                     NR_ATTRIBUTE_DESTINATION_ERROR);
  val = nro_get_hash_string(attrs, "error.group.name", NULL);
  if (val) {
    result = nr_strdup(val);
  }
  nro_delete(attrs);
  return result;
}

static zval* make_exception(TSRMLS_D) {
  return tlib_php_request_eval_expr("new Exception('boom')" TSRMLS_CC);
}

static void test_error_group_callback_not_set(TSRMLS_D) {
  zval* exception;
  char* group_name;

  tlib_php_request_start();
  NRPRG_SHARED(error_group_user_callback).is_set = false;

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_null(
      "callback not set: error.group.name attribute should not be present",
      group_name);

  nr_free(group_name);
  nr_php_zval_free(&exception);
  tlib_php_request_end();
}

static void test_error_group_callback_returns_valid_string(TSRMLS_D) {
  zval* exception;
  char* group_name;

  tlib_php_request_start();
  install_error_group_callback(
      "function ($txn, $err) { return 'mygroup'; }" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_str_equal(
      "callback returns non-empty string: error.group.name should be set",
      "mygroup", group_name);

  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_returns_empty_string(TSRMLS_D) {
  zval* exception;
  char* group_name;

  tlib_php_request_start();
  install_error_group_callback(
      "function ($txn, $err) { return ''; }" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_null(
      "callback returns empty string: error.group.name should not be set",
      group_name);

  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_returns_null(TSRMLS_D) {
  zval* exception;
  char* group_name;

  tlib_php_request_start();
  install_error_group_callback(
      "function ($txn, $err) { return null; }" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_null("callback returns null: error.group.name should not be set",
                    group_name);

  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_returns_non_string(TSRMLS_D) {
  zval* exception;
  char* group_name;

  tlib_php_request_start();
  install_error_group_callback(
      "function ($txn, $err) { return 42; }" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_null(
      "callback returns integer: error.group.name should not be set",
      group_name);

  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_truncates_long_string(TSRMLS_D) {
  zval* exception;
  char* group_name;
  size_t len;

  tlib_php_request_start();
  /*
   * NR_ATTRIBUTE_VALUE_LENGTH_LIMIT is 255. Return a 300-character string
   * and verify the attribute is truncated to exactly that limit.
   */
  install_error_group_callback(
      "function ($txn, $err) { return str_repeat('a', 300); }" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_not_null("long return value: error.group.name should be set",
                        group_name);
  if (group_name) {
    len = nr_strlen(group_name);
    tlib_pass_if_size_t_equal(
        "long return value: error.group.name should be truncated to limit",
        (size_t)NR_ATTRIBUTE_VALUE_LENGTH_LIMIT, len);
  }

  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_null_request_uri(TSRMLS_D) {
  zval* exception;
  char* group_name;
  zval* recorded;

  tlib_php_request_start();
  /*
   * In a CLI test request, $_SERVER['REQUEST_URI'] is not set, so
   * nr_php_get_server_global("REQUEST_URI") returns NULL. This is a
   * regression test for the fix that handles that NULL safely and passes
   * an empty string into the callback's $txn['request_uri'].
   *
   * The callback records the value it received into $GLOBALS so the test
   * can inspect what was actually passed.
   */
  install_error_group_callback(
      "function ($txn, $err) {"
      "  $GLOBALS['cb_uri'] = $txn['request_uri'];"
      "  return 'mygroup';"
      "}" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  /* The attribute should still have been populated -- no crash on NULL URI. */
  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_str_equal(
      "null REQUEST_URI: error.group.name still set from callback return",
      "mygroup", group_name);

  /*
   * The callback should have received an empty string (not null/missing)
   * for request_uri.
   */
  recorded = tlib_php_request_eval_expr("$GLOBALS['cb_uri']" TSRMLS_CC);
  tlib_pass_if_not_null("null REQUEST_URI: callback received a value",
                        recorded);
  tlib_pass_if_int_equal(
      "null REQUEST_URI: callback received a string for request_uri", IS_STRING,
      Z_TYPE_P(recorded));
  tlib_pass_if_str_equal(
      "null REQUEST_URI: callback received empty request_uri string", "",
      Z_STRVAL_P(recorded));

  nr_php_zval_free(&recorded);
  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

static void test_error_group_callback_passes_error_fields(TSRMLS_D) {
  zval* exception;
  char* group_name;
  zval* klass_zv;
  zval* msg_zv;
  zval* file_zv;

  tlib_php_request_start();
  /*
   * Verify the $err array passed to the callback contains the expected
   * klass, message, and file fields drawn from the exception.
   */
  install_error_group_callback(
      "function ($txn, $err) {"
      "  $GLOBALS['cb_klass'] = $err['klass'];"
      "  $GLOBALS['cb_msg'] = $err['message'];"
      "  $GLOBALS['cb_file'] = $err['file'];"
      "  return 'g';"
      "}" TSRMLS_CC);

  exception = make_exception(TSRMLS_C);
  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);

  group_name = lookup_error_group_name(NRPRG(txn));
  tlib_pass_if_not_null("error fields: error.group.name set", group_name);

  klass_zv = tlib_php_request_eval_expr("$GLOBALS['cb_klass']" TSRMLS_CC);
  tlib_pass_if_not_null("error fields: klass captured", klass_zv);
  tlib_pass_if_str_equal("error fields: klass matches exception class name",
                         "Exception", Z_STRVAL_P(klass_zv));

  msg_zv = tlib_php_request_eval_expr("$GLOBALS['cb_msg']" TSRMLS_CC);
  tlib_pass_if_not_null("error fields: message captured", msg_zv);
  tlib_pass_if_str_equal("error fields: message matches exception message",
                         "boom", Z_STRVAL_P(msg_zv));

  file_zv = tlib_php_request_eval_expr("$GLOBALS['cb_file']" TSRMLS_CC);
  tlib_pass_if_not_null("error fields: file captured", file_zv);
  tlib_pass_if_int_equal("error fields: file is a string", IS_STRING,
                         Z_TYPE_P(file_zv));

  nr_php_zval_free(&klass_zv);
  nr_php_zval_free(&msg_zv);
  nr_php_zval_free(&file_zv);
  nr_free(group_name);
  nr_php_zval_free(&exception);
  uninstall_error_group_callback();
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);

  test_error_get_priority();
  test_get_error_type_string();

  test_error_group_callback_not_set(TSRMLS_C);
  test_error_group_callback_returns_valid_string(TSRMLS_C);
  test_error_group_callback_returns_empty_string(TSRMLS_C);
  test_error_group_callback_returns_null(TSRMLS_C);
  test_error_group_callback_returns_non_string(TSRMLS_C);
  test_error_group_callback_truncates_long_string(TSRMLS_C);
  test_error_group_callback_null_request_uri(TSRMLS_C);
  test_error_group_callback_passes_error_fields(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
