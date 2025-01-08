/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_main.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_globals.h"
#include "php_hash.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_extension_loaded(void) {
  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_int_equal("NULL name", 0, nr_php_extension_loaded(NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("empty name", 0, nr_php_extension_loaded(""));
  tlib_pass_if_int_equal("missing extension", 0,
                         nr_php_extension_loaded("foo"));
  tlib_fail_if_int_equal("real extension", 0,
                         nr_php_extension_loaded("standard"));
}

static void test_find_function(TSRMLS_D) {
  zend_function* func;

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL name", nr_php_find_function(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  func = nr_php_find_function("newrelic_get_request_metadata" TSRMLS_CC);
  tlib_pass_if_zend_function_is("name", NULL, "newrelic_get_request_metadata",
                                func);
}

static void test_is_static_function(TSRMLS_D) {
  zend_function* func;
  zend_class_entry* clas;

  tlib_php_request_start();
  /*
   * Test: Invalid/NULL argument
   */
  tlib_pass_if_false("NULL zend_function",
                     nr_php_function_is_static_method(NULL),
                     "Expected null parameter to return false");

  /*
   * Test: When checking a PHP function like newrelic_get_request_metadata, we
   * expect a non static response
   */
  func = nr_php_find_function("newrelic_get_request_metadata" TSRMLS_CC);
  tlib_pass_if_false("PHP Function from global namespace",
                     nr_php_function_is_static_method(func),
                     "Expected PHP function to return false");

  tlib_php_request_eval(
      "class newrelic_test_case_static_and_non_static_tests{public function "
      "instance_method (){} static public function static_method "
      "(){}}" TSRMLS_CC);
  clas = nr_php_find_class(
      "newrelic_test_case_static_and_non_static_tests" TSRMLS_CC);

  /*
   * Test: Static methods should return true
   */
  func = nr_php_find_class_method(clas, "static_method");
  tlib_pass_if_true(
      "Static Method on newrelic_test_case_static_and_non_static_tests",
      nr_php_function_is_static_method(func), "Expected true");

  /*
   * Test: Instance methods should return true
   */
  func = nr_php_find_class_method(clas, "static_method");
  tlib_pass_if_true(
      "Instance Method on newrelic_test_case_static_and_non_static_tests",
      nr_php_function_is_static_method(func), "Expected true");

  tlib_php_request_end();
}

static void test_get_constant(TSRMLS_D) {
  zval* constant;

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL name", nr_php_get_constant(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  constant = nr_php_get_constant("PHP_VERSION" TSRMLS_CC);
  tlib_pass_if_not_null("PHP_VERSION", constant);
  tlib_pass_if_uchar_equal("PHP_VERSION type", IS_STRING, Z_TYPE_P(constant));
  nr_php_zval_free(&constant);

  constant = nr_php_get_constant("NON_EXISTENT_CONSTANT" TSRMLS_CC);
  tlib_pass_if_null("NON_EXISTENT_CONSTANT", constant);
}

static void test_function_debug_name(TSRMLS_D) {
  zval* closure;
  zend_function* func;
  char* name;

  tlib_php_request_start();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL function", nr_php_function_debug_name(NULL));

  /*
   * Test : Normal operation.
   */
  func = nr_php_find_function("date" TSRMLS_CC);
  name = nr_php_function_debug_name(func);
  tlib_pass_if_str_equal("function", "date", name);
  nr_free(name);

  func = nr_php_find_class_method(
      nr_php_find_class("reflectionfunction" TSRMLS_CC), "isdisabled");
  name = nr_php_function_debug_name(func);
  tlib_pass_if_str_equal("method", "ReflectionFunction::isDisabled", name);
  nr_free(name);

  closure = tlib_php_request_eval_expr("function () {}" TSRMLS_CC);
  func = nr_php_zval_to_function(closure TSRMLS_CC);
  name = nr_php_function_debug_name(func);

#if ZEND_MODULE_API_NO < ZEND_8_4_X_API_NO
  tlib_pass_if_str_equal("closure", "{closure} declared at -:1", name);
#else
  tlib_pass_if_str_equal("closure", "{closure:-:1} declared at -:1", name);
#endif

  nr_php_zval_free(&closure);
  nr_free(name);

  tlib_php_request_end();
}

static void setup_inherited_classes(TSRMLS_D) {
  /*
   * PHP 7 will generate deprecation warnings for the code below because of the
   * intentional use of class named constructors. We'll quiet those for now,
   * and we can revisit this when PHP 8 removes support for class named
   * constructors and we have to decide if this test still makes sense.
   */
  tlib_php_request_eval(
      "$er = error_reporting(E_ALL ^ E_DEPRECATED);" TSRMLS_CC);

  tlib_php_request_eval(
      "class A {"
      "public $pub = 'A public';"
      "protected $prot = 'A protected';"
      "private $priv = 'A private';"
      "function a() {}"
      "}"
      "class B extends A {"
      "function b() {}"
      "}"
      "class C extends B {"
      "function __call($name, $args) {}"
      "}" TSRMLS_CC);

  /*
   * Reset the error reporting.
   */
  tlib_php_request_eval("error_reporting($er);" TSRMLS_CC);
}

static void test_get_zval_object_property(TSRMLS_D) {
  zval* obj;
  zval* prop;

  tlib_php_request_start();
  obj = nr_php_zval_alloc();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL object",
                    nr_php_get_zval_object_property(NULL, "pub" TSRMLS_CC));
  tlib_pass_if_null("invalid object",
                    nr_php_get_zval_object_property(obj, "pub" TSRMLS_CC));

  nr_php_zval_free(&obj);

  setup_inherited_classes(TSRMLS_C);
  obj = tlib_php_request_eval_expr("new A" TSRMLS_CC);
  tlib_pass_if_null("NULL name",
                    nr_php_get_zval_object_property(obj, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null("missing property",
                    nr_php_get_zval_object_property(obj, "foo" TSRMLS_CC));

  prop = nr_php_get_zval_object_property(obj, "pub" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A public", Z_STRVAL_P(prop));

  prop = nr_php_get_zval_object_property(obj, "prot" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A protected", Z_STRVAL_P(prop));

  prop = nr_php_get_zval_object_property(obj, "priv" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A private", Z_STRVAL_P(prop));

  /*
   * Test : Can access inherited public and protected properties.
   */
  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new C" TSRMLS_CC);

  prop = nr_php_get_zval_object_property(obj, "pub" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A public", Z_STRVAL_P(prop));

  prop = nr_php_get_zval_object_property(obj, "prot" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A protected", Z_STRVAL_P(prop));

  /*
   * Test : Cannot access inherited private properties.
   */
  tlib_pass_if_null("inherited private",
                    nr_php_get_zval_object_property(obj, "priv" TSRMLS_CC));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_get_zval_object_property_with_class(TSRMLS_D) {
  zend_class_entry* a_ce;
  zend_class_entry* c_ce;
  zend_class_entry* stdclass_ce;
  zval* obj;
  zval* prop;

  tlib_php_request_start();
  obj = nr_php_zval_alloc();

  setup_inherited_classes(TSRMLS_C);
  a_ce = nr_php_find_class("a" TSRMLS_CC);
  c_ce = nr_php_find_class("c" TSRMLS_CC);
  stdclass_ce = nr_php_find_class("stdclass" TSRMLS_CC);

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL object", nr_php_get_zval_object_property_with_class(
                                       NULL, stdclass_ce, "pub" TSRMLS_CC));
  tlib_pass_if_null("invalid object",
                    nr_php_get_zval_object_property_with_class(
                        obj, stdclass_ce, "pub" TSRMLS_CC));

  nr_php_zval_free(&obj);

  obj = tlib_php_request_eval_expr("new A" TSRMLS_CC);
  tlib_pass_if_null(
      "NULL class entry",
      nr_php_get_zval_object_property_with_class(obj, NULL, "pub" TSRMLS_CC));
  tlib_pass_if_null("NULL name", nr_php_get_zval_object_property_with_class(
                                     obj, a_ce, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null(
      "missing property",
      nr_php_get_zval_object_property_with_class(obj, a_ce, "foo" TSRMLS_CC));

  prop = nr_php_get_zval_object_property_with_class(obj, a_ce, "pub" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A public", Z_STRVAL_P(prop));

  prop
      = nr_php_get_zval_object_property_with_class(obj, a_ce, "prot" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A protected", Z_STRVAL_P(prop));

  prop
      = nr_php_get_zval_object_property_with_class(obj, a_ce, "priv" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A private", Z_STRVAL_P(prop));

  /*
   * Test : Can access inherited public and protected properties.
   */
  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new C" TSRMLS_CC);

  prop = nr_php_get_zval_object_property_with_class(obj, c_ce, "pub" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A public", Z_STRVAL_P(prop));

  prop
      = nr_php_get_zval_object_property_with_class(obj, c_ce, "prot" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A protected", Z_STRVAL_P(prop));

  /*
   * Test : Cannot access inherited private properties.
   */
  tlib_pass_if_null(
      "inherited private",
      nr_php_get_zval_object_property_with_class(obj, c_ce, "priv" TSRMLS_CC));

  /*
   * Test : Can access inherited private properties with the right class entry.
   */
  prop
      = nr_php_get_zval_object_property_with_class(obj, a_ce, "priv" TSRMLS_CC);
  tlib_pass_if_zval_type_is("property type", IS_STRING, prop);
  tlib_pass_if_str_equal("property value", "A private", Z_STRVAL_P(prop));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_class_entry_instanceof_class(TSRMLS_D) {
  zend_class_entry* ce = NULL;
  zval* obj;

  tlib_php_request_start();
  setup_inherited_classes(TSRMLS_C);

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_int_equal(
      "NULL ce", 0, nr_php_class_entry_instanceof_class(NULL, "A" TSRMLS_CC));

  obj = tlib_php_request_eval_expr("new B" TSRMLS_CC);
  ce = Z_OBJCE_P(obj);

  tlib_pass_if_int_equal(
      "NULL name", 0, nr_php_class_entry_instanceof_class(ce, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal(
      "non-parent", 0, nr_php_class_entry_instanceof_class(ce, "C" TSRMLS_CC));
  tlib_pass_if_int_equal(
      "non-existent class", 0,
      nr_php_class_entry_instanceof_class(ce, "D" TSRMLS_CC));
  tlib_pass_if_int_equal(
      "same class", 1, nr_php_class_entry_instanceof_class(ce, "B" TSRMLS_CC));
  tlib_pass_if_int_equal(
      "parent class", 1,
      nr_php_class_entry_instanceof_class(ce, "A" TSRMLS_CC));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_object_instanceof_class(TSRMLS_D) {
  zval* obj;

  tlib_php_request_start();
  setup_inherited_classes(TSRMLS_C);

  obj = nr_php_zval_alloc();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_int_equal("NULL object", 0,
                         nr_php_object_instanceof_class(NULL, "A" TSRMLS_CC));
  tlib_pass_if_int_equal("invalid object", 0,
                         nr_php_object_instanceof_class(obj, "A" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new B" TSRMLS_CC);
  tlib_pass_if_int_equal("NULL name", 0,
                         nr_php_object_instanceof_class(obj, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("non-parent", 0,
                         nr_php_object_instanceof_class(obj, "C" TSRMLS_CC));
  tlib_pass_if_int_equal("non-existent class", 0,
                         nr_php_object_instanceof_class(obj, "D" TSRMLS_CC));
  tlib_pass_if_int_equal("same class", 1,
                         nr_php_object_instanceof_class(obj, "B" TSRMLS_CC));
  tlib_pass_if_int_equal("parent class", 1,
                         nr_php_object_instanceof_class(obj, "A" TSRMLS_CC));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_object_has_method(TSRMLS_D) {
  zval* obj;

  tlib_php_request_start();
  setup_inherited_classes(TSRMLS_C);

  obj = nr_php_zval_alloc();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_int_equal("NULL object", 0,
                         nr_php_object_has_method(NULL, "a" TSRMLS_CC));
  tlib_pass_if_int_equal("invalid object", 0,
                         nr_php_object_has_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new A" TSRMLS_CC);
  tlib_pass_if_int_equal("NULL name", 0,
                         nr_php_object_has_method(obj, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("method does not exist on class", 0,
                         nr_php_object_has_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new B" TSRMLS_CC);
  tlib_pass_if_int_equal("method does not exist on class", 0,
                         nr_php_object_has_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_method(obj, "b" TSRMLS_CC));
  tlib_fail_if_int_equal("method inherited by class", 0,
                         nr_php_object_has_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new C" TSRMLS_CC);
  tlib_fail_if_int_equal("class implements __call", 0,
                         nr_php_object_has_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_method(obj, "b" TSRMLS_CC));
  tlib_fail_if_int_equal("method inherited by class", 0,
                         nr_php_object_has_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_object_has_concrete_method(TSRMLS_D) {
  zval* obj;

  tlib_php_request_start();
  setup_inherited_classes(TSRMLS_C);

  obj = nr_php_zval_alloc();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_int_equal(
      "NULL object", 0, nr_php_object_has_concrete_method(NULL, "a" TSRMLS_CC));
  tlib_pass_if_int_equal("invalid object", 0,
                         nr_php_object_has_concrete_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new A" TSRMLS_CC);
  tlib_pass_if_int_equal(
      "NULL name", 0, nr_php_object_has_concrete_method(obj, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal(
      "method does not exist on class", 0,
      nr_php_object_has_concrete_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_concrete_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new B" TSRMLS_CC);
  tlib_pass_if_int_equal(
      "method does not exist on class", 0,
      nr_php_object_has_concrete_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_concrete_method(obj, "b" TSRMLS_CC));
  tlib_fail_if_int_equal("method inherited by class", 0,
                         nr_php_object_has_concrete_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  obj = tlib_php_request_eval_expr("new C" TSRMLS_CC);
  tlib_pass_if_int_equal(
      "class implements __call", 0,
      nr_php_object_has_concrete_method(obj, "foo" TSRMLS_CC));
  tlib_fail_if_int_equal("method exists on class", 0,
                         nr_php_object_has_concrete_method(obj, "b" TSRMLS_CC));
  tlib_fail_if_int_equal("method inherited by class", 0,
                         nr_php_object_has_concrete_method(obj, "a" TSRMLS_CC));

  nr_php_zval_free(&obj);
  tlib_php_request_end();
}

static void test_parse_str(TSRMLS_D) {
  zval* retval;
  zval* value;

  tlib_php_request_start();

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_null("NULL str", nr_php_parse_str(NULL, 0 TSRMLS_CC));
  tlib_pass_if_null("invalid len",
                    nr_php_parse_str(NULL, INT_MAX + 1LL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  retval = nr_php_parse_str(NR_PSTR("") TSRMLS_CC);
  tlib_pass_if_zval_type_is("empty string", IS_ARRAY, retval);
  tlib_pass_if_size_t_equal("empty string", 0,
                            nr_php_zend_hash_num_elements(Z_ARRVAL_P(retval)));
  nr_php_zval_free(&retval);

  retval = nr_php_parse_str(NR_PSTR("a=b&c=d") TSRMLS_CC);
  tlib_pass_if_zval_type_is("query string", IS_ARRAY, retval);
  tlib_pass_if_size_t_equal("query string", 2,
                            nr_php_zend_hash_num_elements(Z_ARRVAL_P(retval)));
  value = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "a");
  tlib_pass_if_zval_type_is("query string a", IS_STRING, value);
  tlib_pass_if_str_equal("query string a", "b", Z_STRVAL_P(value));
  value = nr_php_zend_hash_find(Z_ARRVAL_P(retval), "c");
  tlib_pass_if_zval_type_is("query string c", IS_STRING, value);
  tlib_pass_if_str_equal("query string c", "d", Z_STRVAL_P(value));
  nr_php_zval_free(&retval);

  tlib_php_request_end();
}

static void test_default_address() {
#ifdef NR_SYSTEM_LINUX
  tlib_pass_if_str_equal("default daemon address", "@newrelic",
                         NR_PHP_PROCESS_GLOBALS(address_path));
#else
  tlib_pass_if_str_equal("default daemon address", "/tmp/.newrelic.sock",
                         NR_PHP_PROCESS_GLOBALS(address_path));
#endif
}

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */

static void test_nr_php_zend_function_lineno() {
  zend_function func = {0};

  /*
   * Test : Invalid arguments, NULL zend_execute_data
   */
  tlib_pass_if_uint32_t_equal("NULL zend_execute_data should return 0", 0,
                              nr_php_zend_function_lineno(NULL));

  /*
   * Test : Invalid arguments.
   */
  tlib_pass_if_uint32_t_equal("uninitialized zend_function should return 0", 0,
                              nr_php_zend_function_lineno(&func));

  /*
   * Test : Normal operation.
   */

  func.op_array.line_start = 4;
  func.op_array.type = ZEND_USER_FUNCTION;
  tlib_pass_if_uint32_t_equal("Unexpected lineno name", 4,
                              nr_php_zend_function_lineno(&func));
}

#endif /* PHP 7+ */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  /*
   * Read only tests that can operate within a single, empty request.
   */
  tlib_php_request_start();

  test_extension_loaded();
  test_find_function(TSRMLS_C);
  test_get_constant(TSRMLS_C);
  test_default_address();

  tlib_php_request_end();

  /*
   * Tests that require state and will handle their own request startup and
   * shutdown.
   */

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
  test_nr_php_zend_function_lineno();
#endif /* PHP 7+ */

  test_function_debug_name(TSRMLS_C);
  test_get_zval_object_property(TSRMLS_C);
  test_get_zval_object_property_with_class(TSRMLS_C);
  test_class_entry_instanceof_class(TSRMLS_C);
  test_object_instanceof_class(TSRMLS_C);
  test_object_has_method(TSRMLS_C);
  test_object_has_concrete_method(TSRMLS_C);
  test_parse_str(TSRMLS_C);
  test_is_static_function(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
