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
#include "php_hooks.h"
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
 * nr_php_error_set_error_group_callback takes its own reference to the
 * callable, so the zval it was read from is freed straight away.
 */
static void install_error_group_callback(const char* callable_expr TSRMLS_DC) {
  zend_fcall_info fci;
  zend_fcall_info_cache fcc;
  zval* callable;

  nr_memset(&fci, 0, sizeof(fci));
  nr_memset(&fcc, 0, sizeof(fcc));

  callable = tlib_php_request_eval_expr(callable_expr TSRMLS_CC);
  tlib_pass_if_not_null("install_error_group_callback: callable parsed",
                        callable);

  tlib_pass_if_int_equal(
      "install_error_group_callback: zend_fcall_info_init succeeds", SUCCESS,
      zend_fcall_info_init(callable, 0, &fci, &fcc, NULL, NULL));

  nr_php_error_set_error_group_callback(&fci, &fcc);
  nr_php_zval_free(&callable);
}

static void uninstall_error_group_callback(void) {
  nr_php_error_release_error_group_callback();
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
  tlib_pass_if_null(
      "callback returns null: error.group.name should not be set", group_name);

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
  tlib_pass_if_not_null(
      "long return value: error.group.name should be set", group_name);
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
      "null REQUEST_URI: callback received a string for request_uri",
      IS_STRING, Z_TYPE_P(recorded));
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

/*
 * The tests below register callbacks through newrelic_set_error_group_callback,
 * from PHP, as an application does, and keep no reference of their own: once
 * the registering function returns, only the agent refers to the callback.
 * EgcbCallback adds its name to $GLOBALS['released'] when it's destroyed, so a
 * test can tell when the agent lets go of a callback that holds one.
 */
#define EGCB_CALLBACK_CLASS                                       \
  "class EgcbCallback {"                                          \
  "  public $name;"                                               \
  "  public function __construct($name) { $this->name = $name; }" \
  "  public function __destruct() {"                              \
  "    $GLOBALS['released'][] = $this->name;"                     \
  "  }"                                                           \
  "  public function group($txn, $err) { return $this->name; }"   \
  "  public function replace_self($txn, $err) {"                  \
  "    newrelic_set_error_group_callback("                        \
  "        static function ($txn, $err) {"                        \
  "          return 'replacement';"                               \
  "        });"                                                   \
  "    $GLOBALS['released_during_call']"                          \
  "        = implode(',', $GLOBALS['released']);"                 \
  "    return $this->name;"                                       \
  "  }"                                                           \
  "}"                                                             \
  "$GLOBALS['released'] = array();"

#define EGCB_REGISTER_CLOSURE                     \
  "function egcb_register($name) {"               \
  "  $callback = new EgcbCallback($name);"        \
  "  newrelic_set_error_group_callback("          \
  "      function ($txn, $err) use ($callback) {" \
  "        return $callback->name;"               \
  "      });"                                     \
  "}"

static char* eval_string(const char* expr TSRMLS_DC) {
  zval* value;
  char* result = NULL;

  value = tlib_php_request_eval_expr(expr TSRMLS_CC);
  if (nr_php_is_zval_valid_string(value)) {
    result = nr_strndup(Z_STRVAL_P(value), Z_STRLEN_P(value));
  }
  nr_php_zval_free(&value);
  return result;
}

static char* released_callbacks(TSRMLS_D) {
  return eval_string("implode(',', $GLOBALS['released'])" TSRMLS_CC);
}

static char* record_exception_get_group_name(TSRMLS_D) {
  zval* exception = make_exception(TSRMLS_C);

  nr_php_error_record_exception(NRPRG(txn), exception, 50, false, NULL,
                                NULL TSRMLS_CC);
  nr_php_zval_free(&exception);
  return lookup_error_group_name(NRPRG(txn));
}

static void test_error_group_callback_closure_literal_in_function(TSRMLS_D) {
  char* group_name;

  tlib_php_request_start();
  /*
   * The closures created after egcb_register returns are the same size as the
   * registered one, so they would take its memory if it were freed.
   */
  tlib_php_request_eval(
      "function egcb_register() {"
      "  newrelic_set_error_group_callback(function ($txn, $err) {"
      "    return 'registered';"
      "  });"
      "}"
      "egcb_register();"
      "$others = array();"
      "for ($i = 0; $i < 10; $i++) {"
      "  $others[] = function ($txn, $err) { return 'other'; };"
      "}" TSRMLS_CC);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal(
      "closure literal registered in a function: the agent calls it",
      "registered", group_name);

  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_object_method(TSRMLS_D) {
  char* group_name;
  char* released;

  tlib_php_request_start();
  tlib_php_request_eval(EGCB_CALLBACK_CLASS
                        "function egcb_register() {"
                        "  newrelic_set_error_group_callback("
                        "      array(new EgcbCallback('method'), 'group'));"
                        "}"
                        "egcb_register();" TSRMLS_CC);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal("object method: the agent keeps the object alive", "",
                         released);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal("object method: the agent calls it", "method",
                         group_name);

  nr_free(released);
  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_named_function(TSRMLS_D) {
  char* group_name;

  tlib_php_request_start();
  /*
   * The name is built at run time, so it isn't an interned string, and the
   * agent holds the only reference to it once egcb_register returns.
   */
  tlib_php_request_eval(
      "function egcb_named($txn, $err) { return 'named'; }"
      "function egcb_register() {"
      "  $prefix = 'egcb_';"
      "  newrelic_set_error_group_callback($prefix . 'named');"
      "}"
      "egcb_register();" TSRMLS_CC);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal("named function: the agent calls it", "named",
                         group_name);

  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_replaced(TSRMLS_D) {
  char* group_name;
  char* released;

  tlib_php_request_start();
  tlib_php_request_eval(EGCB_CALLBACK_CLASS EGCB_REGISTER_CLOSURE
                        "egcb_register('first');" TSRMLS_CC);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal("replaced: the first callback is kept until replaced",
                         "", released);
  nr_free(released);

  tlib_php_request_eval("egcb_register('second');" TSRMLS_CC);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal(
      "replaced: the first callback is released, the second is kept", "first",
      released);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal("replaced: the agent calls the second callback",
                         "second", group_name);

  nr_free(released);
  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_registered_twice(TSRMLS_D) {
  char* group_name;
  char* released;

  tlib_php_request_start();
  tlib_php_request_eval(EGCB_CALLBACK_CLASS EGCB_REGISTER_CLOSURE
                        "function egcb_register_twice() {"
                        "  $callback = new EgcbCallback('twice');"
                        "  $closure = function ($txn, $err) use ($callback) {"
                        "    return $callback->name;"
                        "  };"
                        "  newrelic_set_error_group_callback($closure);"
                        "  newrelic_set_error_group_callback($closure);"
                        "}"
                        "egcb_register_twice();" TSRMLS_CC);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal("registered twice: the callback is kept", "",
                         released);
  nr_free(released);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal("registered twice: the agent calls it", "twice",
                         group_name);

  /*
   * One replacement releases the callback only if the agent holds a single
   * reference to it.
   */
  tlib_php_request_eval("egcb_register('replacement');" TSRMLS_CC);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal(
      "registered twice: one replacement releases the callback", "twice",
      released);

  nr_free(released);
  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_replaced_while_running(TSRMLS_D) {
  char* group_name;
  char* released;

  tlib_php_request_start();
  /*
   * Replacing the callback releases the agent's reference to it while it's
   * still running, and then the method uses its object. The replacement is a
   * static closure, so it doesn't keep $this alive itself.
   */
  tlib_php_request_eval(
      EGCB_CALLBACK_CLASS
      "function egcb_register() {"
      "  newrelic_set_error_group_callback("
      "      array(new EgcbCallback('self'), 'replace_self'));"
      "}"
      "egcb_register();" TSRMLS_CC);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal("replaced while running: the callback returns", "self",
                         group_name);

  released = eval_string("$GLOBALS['released_during_call']" TSRMLS_CC);
  tlib_pass_if_str_equal(
      "replaced while running: the object outlives the replacement", "",
      released);
  nr_free(released);

  released = released_callbacks(TSRMLS_C);
  tlib_pass_if_str_equal(
      "replaced while running: the object is released after the call", "self",
      released);
  nr_free(released);
  nr_free(group_name);

  group_name = record_exception_get_group_name(TSRMLS_C);
  tlib_pass_if_str_equal(
      "replaced while running: the agent calls the replacement next",
      "replacement", group_name);

  nr_free(group_name);
  tlib_php_request_end();
}

static void test_error_group_callback_released_at_request_end(TSRMLS_D) {
  tlib_php_request_start();
  tlib_php_request_eval(
      "newrelic_set_error_group_callback(function ($txn, $err) {"
      "  return 'request';"
      "});" TSRMLS_CC);

  tlib_pass_if_true("request end: the callback is set during the request",
                    is_error_callback_set(), "is_set=%d",
                    (int)is_error_callback_set());

  tlib_php_request_end();

  tlib_pass_if_false("request end: no callback is set after the request",
                     is_error_callback_set(), "is_set=%d",
                     (int)is_error_callback_set());
  tlib_pass_if_int_equal(
      "request end: the callable is released", IS_UNDEF,
      Z_TYPE(NRPRG_SHARED(error_group_user_callback).fci.function_name));
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
static int stub_error_cb_type;
static int stub_error_cb_calls;

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
static void stub_error_cb(int type,
                          zend_string* error_filename NRUNUSED,
                          uint error_lineno NRUNUSED,
                          zend_string* message NRUNUSED) {
#else
static void stub_error_cb(int type,
                          const char* error_filename NRUNUSED,
                          uint error_lineno NRUNUSED,
                          zend_string* message NRUNUSED) {
#endif
  stub_error_cb_type = type;
  stub_error_cb_calls += 1;
}

static void test_error_cb_passes_dont_bail_through(TSRMLS_D) {
  nrphperrfn_t saved_cb;
  zend_string* filename;
  zend_string* message;

  tlib_php_request_start();

  saved_cb = NR_PHP_PROCESS_GLOBALS(orig_error_cb);
  NR_PHP_PROCESS_GLOBALS(orig_error_cb) = stub_error_cb;
  stub_error_cb_type = 0;
  stub_error_cb_calls = 0;

  filename = zend_string_init("test.php", sizeof("test.php") - 1, 0);
  message = zend_string_init("boom", sizeof("boom") - 1, 0);

  /*
   * PHP's own handler only bails out on E_ERROR when E_DONT_BAIL is unset, so
   * the agent must hand the type to it unchanged.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  nr_php_error_cb(E_ERROR | E_DONT_BAIL, filename, 1, message);
#else
  nr_php_error_cb(E_ERROR | E_DONT_BAIL, ZSTR_VAL(filename), 1, message);
#endif

  tlib_pass_if_int_equal("original handler called once", 1,
                         stub_error_cb_calls);
  tlib_pass_if_int_equal("E_DONT_BAIL is preserved for the original handler",
                         E_ERROR | E_DONT_BAIL, stub_error_cb_type);

  NR_PHP_PROCESS_GLOBALS(orig_error_cb) = saved_cb;
  zend_string_release(filename);
  zend_string_release(message);
  tlib_php_request_end();
}
#endif /* PHP >= 8.0 */

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
  test_error_group_callback_closure_literal_in_function(TSRMLS_C);
  test_error_group_callback_object_method(TSRMLS_C);
  test_error_group_callback_named_function(TSRMLS_C);
  test_error_group_callback_replaced(TSRMLS_C);
  test_error_group_callback_registered_twice(TSRMLS_C);
  test_error_group_callback_replaced_while_running(TSRMLS_C);
  test_error_group_callback_released_at_request_end(TSRMLS_C);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_error_cb_passes_dont_bail_through(TSRMLS_C);
#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
