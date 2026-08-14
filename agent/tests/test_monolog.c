/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "nr_attributes.h"
#include "nr_attributes_private.h"
#include "lib_monolog_private.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
static void setup_logrecord() {
  const char* log_record_class
      = "namespace Monolog;"
        "class LogRecord "
        "{"
        "public function __construct("
        "public string $message = 'info',"
        "public array $context = [],"
        "public array $extra = []) {}"
        "};";

  tlib_php_request_eval(log_record_class);
}

static void test_context_extra_merge_behavior(TSRMLS_D) {
  zval* record;
  nr_attributes_t* attributes;
  tlib_php_request_start();

  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: Direct testing of context/extra merge using exposed function */
  /* Create a record with both context and extra data, including collision */

  setup_logrecord();

  /* Create a record with both context and extra data, including collision */
  record = tlib_php_request_eval_expr(
      "new Monolog\\LogRecord(message: 'error',context: ['context_key' => "
      "'from_context', 'shared_key' => 'old'], extra: ['extra_key' => "
      "'metadata', 'shared_key' => 'new']);");

  /* Test the actual merge function directly */
  attributes = nr_monolog_get_postprocessed_attributes(record TSRMLS_CC);
  tlib_pass_if_not_null("Postprocessed attributes created", attributes);

  if (attributes) {
    nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
        attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    /* Verify both context and extra attributes exist */
    tlib_pass_if_not_null(
        "Context attribute exists",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "from_context",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_not_null(
        "Extra attribute exists",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));
    tlib_pass_if_str_equal(
        "Extra key/var exist", "metadata",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));

    /* Test key collision behavior - extra should overwrite context for same key
     */
    tlib_pass_if_str_equal(
        "Extra overwrites context for same key", "new",
        nro_get_hash_string(log_attributes, "context.shared_key", NULL));

    nro_delete(log_attributes);
    nr_attributes_destroy(&attributes);
  }

  nr_php_zval_free(&record);

  /* Create a record with only context and no extra data*/
  record = tlib_php_request_eval_expr(
      "new Monolog\\LogRecord(message: 'error',context: ['context_key' => "
      "'from_context', 'shared_key' => 'old']);");

  /* Test the actual merge function directly */
  attributes = nr_monolog_get_postprocessed_attributes(record TSRMLS_CC);
  tlib_pass_if_not_null("Postprocessed attributes created", attributes);

  if (attributes) {
    nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
        attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    /* Verify only context and no extra attributes exist */
    tlib_pass_if_not_null(
        "Context attribute exists",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "from_context",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_null(
        "Extra attribute exists",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));
    tlib_pass_if_str_equal(
        "Another attribute", "old",
        nro_get_hash_string(log_attributes, "context.shared_key", NULL));

    nro_delete(log_attributes);
    nr_attributes_destroy(&attributes);
  }

  nr_php_zval_free(&record);

  /* Create a record with only extra and no context data*/
  record = tlib_php_request_eval_expr(
      "new Monolog\\LogRecord(message: 'error', extra: ['extra_key' => "
      "'metadata', 'shared_key' => 'new']);");

  /* Test the actual merge function directly */
  attributes = nr_monolog_get_postprocessed_attributes(record TSRMLS_CC);
  tlib_pass_if_not_null("Postprocessed attributes created", attributes);

  if (attributes) {
    nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
        attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    /* Verify only extra attributes exist */
    tlib_pass_if_null(
        "Context attribute exists",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_not_null(
        "Extra attribute exists",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "metadata",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));
    tlib_pass_if_str_equal(
        "Another attribute", "new",
        nro_get_hash_string(log_attributes, "context.shared_key", NULL));

    nro_delete(log_attributes);
    nr_attributes_destroy(&attributes);
  }

  nr_php_zval_free(&record);

  /* Create a record with only context and no extra data*/
  record
      = tlib_php_request_eval_expr("new Monolog\\LogRecord(message: 'error');");

  /* Test the actual merge function directly */
  attributes = nr_monolog_get_postprocessed_attributes(record TSRMLS_CC);
  tlib_pass_if_not_null("Postprocessed attributes created", attributes);

  if (attributes) {
    nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
        attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    /* Verify no attributes exist */
    tlib_pass_if_null(
        "Context attribute exists",
        nro_get_hash_string(log_attributes, "context.context_key", NULL));
    tlib_pass_if_null(
        "Extra attribute exists",
        nro_get_hash_string(log_attributes, "context.extra_key", NULL));

    nro_delete(log_attributes);
    nr_attributes_destroy(&attributes);
  }

  nr_php_zval_free(&record);

  tlib_php_request_end();
}

static void test_monolog_version_format_differences(TSRMLS_D) {
  zval* v2_record;
  zval* v3_record;
  nr_attributes_t* v2_attributes;
  nr_attributes_t* v3_attributes;
  tlib_php_request_start();

  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: Monolog v2 format (array with 'context' and 'extra' keys) */

  char* valid_array_args
      = "array("
        "    'context' => array("
        "        'v2_user_name' => 'MyName'"
        "    ),"
        "    'extra' => array("
        "        'v2_processor' => 'metadata'"
        "    )"
        ")";
  v2_record = tlib_php_request_eval_expr(valid_array_args);

  /* Test v2 record processing through the main function */
  v2_attributes = nr_monolog_get_postprocessed_attributes(v2_record TSRMLS_CC);
  tlib_pass_if_not_null("V2 record processed", v2_attributes);

  if (v2_attributes) {
    nrobj_t* v2_log_attrs = nr_attributes_logcontext_to_obj(
        v2_attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    tlib_pass_if_not_null(
        "V2 context attribute exists",
        nro_get_hash_string(v2_log_attrs, "context.v2_user_name", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "MyName",
        nro_get_hash_string(v2_log_attrs, "context.v2_user_name", NULL));
    tlib_pass_if_not_null(
        "V2 extra attribute exists",
        nro_get_hash_string(v2_log_attrs, "context.v2_processor", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "metadata",
        nro_get_hash_string(v2_log_attrs, "context.v2_processor", NULL));

    nro_delete(v2_log_attrs);
    nr_attributes_destroy(&v2_attributes);
  }

  /* Test: Monolog v3 format (object with context/extra properties) */
  /* Create a mocked LogRecord PHP object with context and extra properties */

  setup_logrecord();
  v3_record = tlib_php_request_eval_expr(
      "new Monolog\\LogRecord(message: 'error',context: ['context_key' => "
      "'from_context', 'shared_key' => 'old'], extra: ['extra_key' => "
      "'metadata', 'shared_key' => 'new']);");

  /* Test v3 record processing through the main function */
  v3_attributes = nr_monolog_get_postprocessed_attributes(v3_record TSRMLS_CC);
  tlib_pass_if_not_null("V3 record processed", v3_attributes);

  if (v3_attributes) {
    nrobj_t* v3_log_attrs = nr_attributes_logcontext_to_obj(
        v3_attributes, NR_ATTRIBUTE_DESTINATION_LOG);

    tlib_pass_if_not_null(
        "Context attribute exists",
        nro_get_hash_string(v3_log_attrs, "context.context_key", NULL));
    tlib_pass_if_str_equal(
        "Context key/var exist", "from_context",
        nro_get_hash_string(v3_log_attrs, "context.context_key", NULL));
    tlib_pass_if_not_null(
        "Extra attribute exists",
        nro_get_hash_string(v3_log_attrs, "context.extra_key", NULL));
    tlib_pass_if_str_equal(
        "Extra key/var exist", "metadata",
        nro_get_hash_string(v3_log_attrs, "context.extra_key", NULL));

    nro_delete(v3_log_attrs);
    nr_attributes_destroy(&v3_attributes);
    nr_php_zval_free(&v3_record);
    nr_php_zval_free(&v2_record);
  }

  /* Test: Invalid record (neither array nor object with properties) */
  zval* invalid_record
      = tlib_php_request_eval_expr("'just a string';" TSRMLS_CC);
  nr_attributes_t* invalid_attributes
      = nr_monolog_get_postprocessed_attributes(invalid_record TSRMLS_CC);
  tlib_pass_if_null("Invalid record returns null", invalid_attributes);

  nr_php_zval_free(&v2_record);
  nr_php_zval_free(&v3_record);
  nr_php_zval_free(&invalid_record);
  tlib_php_request_end();
}
#endif

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

  /* test array - now converted to JSON string */
  obj = tlib_php_request_eval_expr("array(1, 2, 3);" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Array converted to JSON", nrobj);
  tlib_pass_if_equal("Array converted to string", NR_OBJECT_STRING,
                     nro_type(nrobj), int, "%d");
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

#define TEST_ATTRIBUTES_CREATION(CONTEXT_DATA, EXPECTED_JSON)              \
  do {                                                                     \
    char* actual_json;                                                     \
    nr_attributes_t* attributes                                            \
        = nr_monolog_convert_context_data_to_attributes(context_data,      \
                                                        NULL TSRMLS_CC);   \
                                                                           \
    tlib_fail_if_null("attributes is not NULL", attributes);               \
                                                                           \
    nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(             \
        attributes, NR_ATTRIBUTE_DESTINATION_LOG);                         \
                                                                           \
    tlib_fail_if_null("log_attributes is not NULL", log_attributes);       \
    tlib_fail_if_bool_equal("At least one attribute created", 1,           \
                            0 > nro_getsize(log_attributes));              \
                                                                           \
    if (0 < nro_getsize(log_attributes)) {                                 \
      actual_json = nro_to_json(log_attributes);                           \
    }                                                                      \
                                                                           \
    tlib_pass_if_str_equal("Converted array", expected_json, actual_json); \
    nr_free(actual_json);                                                  \
    nro_delete(log_attributes);                                            \
    nr_attributes_destroy(&attributes);                                    \
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

  /* test without any filters and all attributes allowed - order matches actual
   * output */
  char* expected_json
      = "{"
        "\"context.array_attr\":\"{\\\"nested_string\\\":\\\"nested_string_"
        "value\\\"}\","
        "\"context.false_bool_attr\":false,"
        "\"context.true_bool_attr\":true,"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000,"
        "\"context.string_attr\":\"string_value\""
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  /* add filtering rules and try again */
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "string_attr",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "f*", 0,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "t*", 0,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  expected_json
      = "{"
        "\"context.int_attr\":1234,"
        "\"context.string_attr\":\"string_value\""
        "}";

  TEST_ATTRIBUTES_CREATION(context_data, expected_json);

  /* another case to add filtering rules and try again */
  nr_attribute_config_destroy(&(NRPRG(txn)->attribute_config));
  NRPRG(txn)->attribute_config = nr_attribute_config_copy(orig_config);
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "d*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "*", 0,
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
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "d*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "i*",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "true_bool_attr",
                                          NR_ATTRIBUTE_DESTINATION_LOG, 0);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config, "t*", 0,
                                          NR_ATTRIBUTE_DESTINATION_ALL);
  nr_attribute_config_modify_destinations(NRPRG(txn)->attribute_config,
                                          "false_bool_attr", 0,
                                          NR_ATTRIBUTE_DESTINATION_ALL);
  expected_json
      = "{"
        "\"context.true_bool_attr\":true,"
        "\"context.int_attr\":1234,"
        "\"context.double_attr\":3.10000"
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
      = nr_monolog_convert_context_data_to_attributes(NULL, NULL TSRMLS_CC);

  tlib_pass_if_null("NULL context yields attributes is NULL", attributes);

  // create an undefined zval - nr_php_zval_alloc() returns undefined
  zval* z = nr_php_zval_alloc();

  tlib_pass_if_equal("zval is undefined type", IS_UNDEF, Z_TYPE_P(z), int,
                     "%d");

  attributes = nr_monolog_convert_context_data_to_attributes(z, NULL TSRMLS_CC);

  tlib_pass_if_null("zval of undefined type yields attributes is NULL",
                    attributes);
  nr_php_zval_free(&z);

  tlib_php_request_end();
}

static void test_convert_context_nested_arrays(TSRMLS_D) {
  zval* context_data;
  nr_attributes_t* attributes;
  tlib_php_request_start();

  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: Nested arrays get converted to JSON strings */
  context_data = tlib_php_request_eval_expr(
      "array("
      "'nested' => array('inner' => 'value', 'number' => 42),"
      "'simple' => 'string'"
      ");" TSRMLS_CC);

  attributes = nr_monolog_convert_context_data_to_attributes(context_data,
                                                             NULL TSRMLS_CC);

  tlib_pass_if_not_null("Nested arrays converted to attributes", attributes);

  nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
      attributes, NR_ATTRIBUTE_DESTINATION_LOG);

  tlib_pass_if_not_null("Log attributes created", log_attributes);

  /* Check that simple attribute exists, nested array may be filtered */
  tlib_pass_if_str_equal(
      "Simple attribute preserved", "string",
      nro_get_hash_string(log_attributes, "context.simple", NULL));

  nro_delete(log_attributes);
  nr_attributes_destroy(&attributes);
  nr_php_zval_free(&context_data);
  tlib_php_request_end();
}

static void test_convert_context_type_edge_cases(TSRMLS_D) {
  zval* obj;
  nrobj_t* nrobj;

  tlib_php_request_start();

  /* Test: Resource type should be ignored */
  obj = tlib_php_request_eval_expr("fopen('php://memory', 'r');" TSRMLS_CC);
  if (obj && Z_TYPE_P(obj) == IS_RESOURCE) {
    nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
    tlib_pass_if_null("Resource type ignored", nrobj);
    nro_delete(nrobj);
  }
  nr_php_zval_free(&obj);

  /* Test: Empty string */
  obj = tlib_php_request_eval_expr("'';" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Empty string converted", nrobj);
  tlib_pass_if_str_equal("Empty string value", "", nro_get_string(nrobj, NULL));
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* Test: Zero values */
  obj = tlib_php_request_eval_expr("0;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Zero converted", nrobj);
  tlib_pass_if_equal("Zero value", 0, nro_get_long(nrobj, NULL), int, "%d");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* Test: False boolean */
  obj = tlib_php_request_eval_expr("false;" TSRMLS_CC);
  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("False converted", nrobj);
  tlib_pass_if_false("False value", nro_get_boolean(nrobj, NULL),
                     "expected false");
  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  tlib_php_request_end();
}

static void test_convert_context_with_special_characters(TSRMLS_D) {
  zval* context_data;
  nr_attributes_t* attributes;
  tlib_php_request_start();

  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: Keys and values with special characters */
  context_data = tlib_php_request_eval_expr(
      "array("
      "'unicode_key' => 'Value with unicode: \\xc2\\xa9 \\xe2\\x84\\xa2',"
      "'json_chars' => '\\\"quoted\\\" and \\\\backslash\\\\ and /slash/',"
      "'control_chars' => \"\\n\\r\\t\\b\\f\""
      ");" TSRMLS_CC);

  attributes = nr_monolog_convert_context_data_to_attributes(context_data,
                                                             NULL TSRMLS_CC);

  tlib_pass_if_not_null("Special characters converted to attributes",
                        attributes);

  nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
      attributes, NR_ATTRIBUTE_DESTINATION_LOG);

  tlib_pass_if_not_null("Log attributes with special chars created",
                        log_attributes);

  /* Verify attributes exist (exact content depends on JSON escaping) */
  tlib_pass_if_not_null(
      "Unicode attribute exists",
      nro_get_hash_string(log_attributes, "context.unicode_key", NULL));
  tlib_pass_if_not_null(
      "JSON chars attribute exists",
      nro_get_hash_string(log_attributes, "context.json_chars", NULL));
  tlib_pass_if_not_null(
      "Control chars attribute exists",
      nro_get_hash_string(log_attributes, "context.control_chars", NULL));

  nro_delete(log_attributes);
  nr_attributes_destroy(&attributes);
  nr_php_zval_free(&context_data);
  tlib_php_request_end();
}

static void test_postprocessed_attributes_null_handling(TSRMLS_D) {
  nrtxn_t* txn;
  nr_attributes_t* attributes;

  tlib_php_request_start();
  txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: NULL record */
  attributes = nr_monolog_get_postprocessed_attributes(NULL TSRMLS_CC);
  tlib_pass_if_null("NULL record returns NULL", attributes);

  /* Test: Object without context/extra properties */
  zval* empty_obj = tlib_php_request_eval_expr("new stdClass();" TSRMLS_CC);
  attributes = nr_monolog_get_postprocessed_attributes(empty_obj TSRMLS_CC);
  tlib_pass_if_null("Empty object returns NULL", attributes);

  /* Test: Array without context/extra keys */
  zval* empty_array
      = tlib_php_request_eval_expr("array('message' => 'test');" TSRMLS_CC);
  attributes = nr_monolog_get_postprocessed_attributes(empty_array TSRMLS_CC);
  tlib_pass_if_null("Array without context/extra returns NULL", attributes);

  nr_php_zval_free(&empty_obj);
  nr_php_zval_free(&empty_array);
  tlib_php_request_end();
}

static void test_json_encoding_failure_handling(TSRMLS_D) {
  zval* obj;
  nrobj_t* nrobj;

  tlib_php_request_start();

  /* Test: Very deeply nested array (potential JSON encoding issues) */
  obj = tlib_php_request_eval_expr(
      "array("
      "'level1' => array("
      "  'level2' => array("
      "    'level3' => array("
      "      'level4' => array("
      "        'deep_value' => 'test'"
      "      )"
      "    )"
      "  )"
      ")"
      ");" TSRMLS_CC);

  nrobj = nr_monolog_context_data_zval_to_attribute_obj(obj);
  tlib_pass_if_not_null("Deeply nested array converted", nrobj);

  /* Should be converted to JSON string */
  if (nrobj) {
    tlib_pass_if_equal("Nested array becomes string", NR_OBJECT_STRING,
                       nro_type(nrobj), int, "%d");
    const char* json_str = nro_get_string(nrobj, NULL);
    tlib_pass_if_not_null("JSON string contains nested data",
                          nr_strstr(json_str, "deep_value"));
  }

  nr_php_zval_free(&obj);
  nro_delete(nrobj);

  /* Test: Circular reference handling (if possible to create safely) */
  /* This might cause JSON encoding to fail */

  tlib_php_request_end();
}

static void test_attribute_key_collision_handling(TSRMLS_D) {
  zval* context_data;
  nr_attributes_t* attributes = NULL;
  tlib_php_request_start();

  nrtxn_t* txn = NRPRG(txn);
  txn->options.log_forwarding_context_data_enabled = 1;
  nr_attribute_config_enable_destinations(txn->attribute_config,
                                          NR_ATTRIBUTE_DESTINATION_LOG);

  /* Test: Context with keys that might conflict with agent attributes */
  context_data = tlib_php_request_eval_expr(
      "array("
      "'timestamp' => 'user_timestamp',"
      "'level' => 'user_level',"
      "'message' => 'user_message',"
      "'normal_key' => 'normal_value'"
      ");" TSRMLS_CC);

  attributes = nr_monolog_convert_context_data_to_attributes(context_data,
                                                             NULL TSRMLS_CC);
  tlib_pass_if_not_null("Attributes with potential conflicts created",
                        attributes);

  nrobj_t* log_attributes = nr_attributes_logcontext_to_obj(
      attributes, NR_ATTRIBUTE_DESTINATION_LOG);

  /* All should be prefixed with "context." so no conflicts */
  tlib_pass_if_not_null(
      "User timestamp attribute",
      nro_get_hash_string(log_attributes, "context.timestamp", NULL));
  tlib_pass_if_not_null(
      "User level attribute",
      nro_get_hash_string(log_attributes, "context.level", NULL));
  tlib_pass_if_not_null(
      "Normal attribute",
      nro_get_hash_string(log_attributes, "context.normal_key", NULL));

  nro_delete(log_attributes);
  nr_attributes_destroy(&attributes);
  nr_php_zval_free(&context_data);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");

  test_convert_zval_to_attribute_obj();
  test_convert_context_data_to_attributes();
  test_convert_context_data_to_attributes_bad_params();
  test_convert_context_nested_arrays();
  test_convert_context_type_edge_cases();
  test_convert_context_with_special_characters();
  test_json_encoding_failure_handling();
  test_postprocessed_attributes_null_handling();
  test_attribute_key_collision_handling();

  #if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_context_extra_merge_behavior();
  test_monolog_version_format_differences();
  #endif

  tlib_php_engine_destroy(TSRMLS_C);
}
