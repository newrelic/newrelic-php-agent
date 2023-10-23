/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "nr_attributes.h"
#include "lib_monolog_private.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_convert_zval_to_attribute_obj(TSRMLS_D) {
  zval* obj;
  nrobj_t* nrobj;
  nr_status_t err;

  tlib_php_request_start();

  /* test null zval */
  obj = nr_php_zval_alloc();
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_null("NULL zval", nrobj);
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* test boolean */
  obj = tlib_php_request_eval_expr("True;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Boolean converted", nrobj);
  tlib_pass_if_equal("Boolean type correct", NR_OBJECT_BOOLEAN, nro_type(nrobj),
                     int, "%d");
  tlib_pass_if_true("Boolean value correct", nro_get_boolean(nrobj, &err),
                    "expected true");
  tlib_pass_if_equal("Boolean GET successful", NR_SUCCESS, err, int, "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* long */
  obj = tlib_php_request_eval_expr("1234567;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Long converted", nrobj);
  tlib_pass_if_equal("Long type correct", NR_OBJECT_LONG, nro_type(nrobj), int,
                     "%d");
  tlib_pass_if_equal("Long value correct", 1234567, nro_get_long(nrobj, &err),
                     int, "%d");
  tlib_pass_if_equal("Long GET successful", NR_SUCCESS, err, int, "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* double */
  obj = tlib_php_request_eval_expr("1.234567;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Double converted", nrobj);
  tlib_pass_if_equal("Double type correct", NR_OBJECT_DOUBLE, nro_type(nrobj),
                     int, "%d");
  tlib_pass_if_equal("Double value correct", 1.234567,
                     nro_get_double(nrobj, &err), int, "%d");
  tlib_pass_if_equal("Double GET successful", NR_SUCCESS, err, int, "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* string */
  obj = tlib_php_request_eval_expr("\"A\";" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("String converted", nrobj);
  tlib_pass_if_equal("String type correct", NR_OBJECT_STRING, nro_type(nrobj),
                     int, "%d");
  tlib_pass_if_str_equal("String value correct", "A",
                         nro_get_string(nrobj, &err));
  tlib_pass_if_equal("String GET successful", NR_SUCCESS, err, int, "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* constant boolean */
  tlib_php_request_eval("define(\"CONSTANT_DEFINE_BOOLEAN\", True);" TSRMLS_CC);
  obj = tlib_php_request_eval_expr("CONSTANT_DEFINE_BOOLEAN;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Constant Boolean converted", nrobj);
  tlib_pass_if_equal("Constant Boolean type correct", NR_OBJECT_BOOLEAN,
                     nro_type(nrobj), int, "%d");
  tlib_pass_if_true("Constant Boolean value correct",
                    nro_get_boolean(nrobj, &err), "expected true");
  tlib_pass_if_equal("Constant Boolean GET successful", NR_SUCCESS, err, int,
                     "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* constant long */
  tlib_php_request_eval("define(\"CONSTANT_DEFINE_LONG\",1234567);" TSRMLS_CC);
  obj = tlib_php_request_eval_expr("CONSTANT_DEFINE_LONG;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Constant Long converted", nrobj);
  tlib_pass_if_equal("Constant Long type correct", NR_OBJECT_LONG,
                     nro_type(nrobj), int, "%d");
  tlib_pass_if_equal("Constant Long value correct", 1234567,
                     nro_get_long(nrobj, &err), int, "%d");
  tlib_pass_if_equal("Constant Long GET successful", NR_SUCCESS, err, int,
                     "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* double */
  tlib_php_request_eval(
      "define(\"CONSTANT_DEFINE_DOUBLE\",1.234567);" TSRMLS_CC);
  obj = tlib_php_request_eval_expr("CONSTANT_DEFINE_DOUBLE;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Constant Double converted", nrobj);
  tlib_pass_if_equal("Constant Double type correct", NR_OBJECT_DOUBLE,
                     nro_type(nrobj), int, "%d");
  tlib_pass_if_equal("Constant Double value correct", 1.234567,
                     nro_get_double(nrobj, &err), int, "%d");
  tlib_pass_if_equal("Constant Double GET successful", NR_SUCCESS, err, int,
                     "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* test constant string */
  tlib_php_request_eval("define(\"CONSTANT_DEFINE_STRING\", \"A\");" TSRMLS_CC);
  obj = tlib_php_request_eval_expr("CONSTANT_DEFINE_STRING;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Constant String converted", nrobj);
  tlib_pass_if_equal("Constant tring type correct", NR_OBJECT_STRING,
                     nro_type(nrobj), int, "%d");
  tlib_pass_if_str_equal("Constant String value correct", "A",
                         nro_get_string(nrobj, &err));
  tlib_pass_if_equal("Constant String GET successful", NR_SUCCESS, err, int,
                     "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* test array */
  obj = tlib_php_request_eval_expr("array(1, 2, 3);" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_null("Array not converted", nrobj);
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* test object */
  obj = tlib_php_request_eval_expr("new stdClass();" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_null("Object not converted", nrobj);
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  tlib_php_request_end();
}

#define TEST_ATTRIBUTES_CREATION(CONTEXT_DATA, EXPECTED_JSON)                  \
  do {                                                                         \
    char* actual_json;                                                         \
    nr_attributes_t* attributes                                                \
        = nr_monolog_convert_context_data_to_attributes(context_data);         \
                                                                               \
    tlib_fail_if_null("attributes is not NULL", attributes);                   \
                                                                               \
    nrobj_t* log_attributes                                                    \
        = nr_attributes_user_to_obj(attributes, NR_ATTRIBUTE_DESTINATION_LOG); \
                                                                               \
    tlib_fail_if_null("log_attributes is not NULL", log_attributes);           \
    tlib_fail_if_bool_equal("At least one attribute created", 1,               \
                            0 > nro_getsize(log_attributes));                  \
                                                                               \
    if (0 < nro_getsize(log_attributes)) {                                     \
      actual_json = nro_to_json(log_attributes);                               \
    }                                                                          \
                                                                               \
    tlib_pass_if_str_equal("Converted array", expected_json, actual_json);     \
    nr_free(actual_json);                                                      \
    nro_delete(log_attributes);                                                \
    nr_attributes_destroy(&attributes);                                        \
  } while (0)

static void test_convert_context_data_to_attributes(TSRMLS_D) {
  zval* context_data;

  tlib_php_request_start();
  nrtxn_t* txn = NRPRG(txn);

  /* enable context data filtering */
  nr_attribute_config_t* orig_config
      = nr_attribute_config_copy(NRPRG(txn)->attribute_config);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  context_data = tlib_php_request_eval_expr(
      "array("
      "1=>\"one\","
      "\"null_attr\"=>null,"
      "\"string_attr\"=>\"string_value\","
      "\"double_attr\"=>3.1,"
      "\"int_attr\"=>1234,"
      "\"true_bool_attr\"=>True,"
      "\"false_bool_attr\"=>False,"
      "\"array_attr\"=>array(\"nested_string\"=>\"nested_string_value\"),"
      "\"object_attr\"=>new StdClass())" TSRMLS_CC);

  /* test without any filters and all attributes allowed */
  char* expected_json
      = "{"
        "\"context.false_bool_attr\":false,"
        "\"context.true_bool_attr\":true,"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000,"
        "\"context.string_attr\":\"string_value\""
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  /* add filtering rules and try again */
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.string_attr",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.f*", 0,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.t*", 0,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  expected_json
      = "{"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000,"
        "\"context.string_attr\":\"string_value\""
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  /* another case to add filtering rules and try again */
  nr_attribute_config_destroy(&(NRPRG(txn)->attribute_config));
  NRPRG(txn)->attribute_config = nr_attribute_config_copy(orig_config);
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.d*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.*", 0,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  expected_json
      = "{"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000"
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  /* test global and context_data include/exclude rules */
  nr_attribute_config_destroy(&(NRPRG(txn)->attribute_config));
  NRPRG(txn)->attribute_config = nr_attribute_config_copy(orig_config);
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.d*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.true_bool_attr",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.t*", 0,
                                          NR_ATTRIBUTE_DESTINATION_ALL);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "context.false_bool_attr", 0,
                                          NR_ATTRIBUTE_DESTINATION_ALL);
  expected_json
      = "{"
        "\"context.true_bool_attr\":true,"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000,"
        "\"context.string_attr\":\"string_value\""
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  nr_attribute_config_destroy(&orig_config);
  nr_php_zval_free(&context_data);

  tlib_php_request_end();
}

static void test_convert_context_data_to_attributes_bad_params(TSRMLS_D) {
  tlib_php_request_start();

  /* enable context data destination */
  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  nr_attributes_t* attributes
      = nr_monolog_convert_context_data_to_attributes(NULL);

  tlib_pass_if_null("NULL context yields attributes is NULL", attributes);

  // create an undefined zval - nr_php_zval_alloc() returns undefined
  zval* z = nr_php_zval_alloc();

  tlib_pass_if_equal("zval is undefined type", IS_UNDEF, Z_TYPE_P(z), int, "%d");

  attributes = nr_monolog_convert_context_data_to_attributes(z);

  tlib_pass_if_null("zval of undefined type yields attributes is NULL",
                    attributes);
  nr_php_zval_free(&z);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {

  tlib_php_engine_create("");

  test_convert_zval_to_attribute_obj();
  test_convert_context_data_to_attributes();
  test_convert_context_data_to_attributes_bad_params();

  tlib_php_engine_destroy(TSRMLS_C);
}
