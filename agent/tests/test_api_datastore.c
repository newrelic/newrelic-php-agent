/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_api_datastore.h"
#include "php_api_datastore_private.h"
#include "php_hash.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_create_instance_from_params(TSRMLS_D) {
  nr_datastore_instance_t* instance;
  zval* params;

  tlib_php_request_start();

  params = tlib_php_request_eval_expr("array()" TSRMLS_CC);
  instance = nr_php_api_datastore_create_instance_from_params(params);
  tlib_pass_if_str_equal("empty params: host", "unknown",
                         nr_datastore_instance_get_host(instance));
  tlib_pass_if_str_equal("empty params: port", "unknown",
                         nr_datastore_instance_get_port_path_or_id(instance));
  tlib_pass_if_str_equal("empty params: database", "unknown",
                         nr_datastore_instance_get_database_name(instance));
  nr_datastore_instance_destroy(&instance);
  nr_php_zval_free(&params);

  params = tlib_php_request_eval_expr(
      "array('databaseName' => 1, 'host' => 2, 'portPathOrId' => 3)" TSRMLS_CC);
  instance = nr_php_api_datastore_create_instance_from_params(params);
  tlib_pass_if_str_equal("invalid params: host", "unknown",
                         nr_datastore_instance_get_host(instance));
  tlib_pass_if_str_equal("invalid params: port", "unknown",
                         nr_datastore_instance_get_port_path_or_id(instance));
  tlib_pass_if_str_equal("invalid params: database", "unknown",
                         nr_datastore_instance_get_database_name(instance));
  nr_datastore_instance_destroy(&instance);
  nr_php_zval_free(&params);

  params = tlib_php_request_eval_expr(
      "array('databaseName' => 'db', 'host' => 'host.name', 'portPathOrId' => "
      "'3333')" TSRMLS_CC);
  instance = nr_php_api_datastore_create_instance_from_params(params);
  tlib_pass_if_str_equal("valid params: host", "host.name",
                         nr_datastore_instance_get_host(instance));
  tlib_pass_if_str_equal("valid params: port", "3333",
                         nr_datastore_instance_get_port_path_or_id(instance));
  tlib_pass_if_str_equal("valid params: database", "db",
                         nr_datastore_instance_get_database_name(instance));
  nr_datastore_instance_destroy(&instance);
  nr_php_zval_free(&params);

  tlib_php_request_end();
}

static void test_validate(TSRMLS_D) {
  zval* input;
  zval* output;

  tlib_php_request_start();

  input = tlib_php_request_eval_expr("array()" TSRMLS_CC);
  output = nr_php_api_datastore_validate(Z_ARRVAL_P(input));
  tlib_pass_if_null("empty params", output);
  nr_php_zval_free(&input);

  input = tlib_php_request_eval_expr("array('product' => 42)" TSRMLS_CC);
  output = nr_php_api_datastore_validate(Z_ARRVAL_P(input));
  tlib_pass_if_not_null("coerced params", output);
  tlib_pass_if_zval_type_is("coerced params: type", IS_ARRAY, output);
  tlib_pass_if_size_t_equal("coerced params: size", 3,
                            nr_php_zend_hash_num_elements(Z_ARRVAL_P(output)));
  tlib_pass_if_str_equal(
      "coerced params: product", "42",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "product")));
  tlib_pass_if_str_equal(
      "coerced params: collection", "other",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "collection")));
  tlib_pass_if_str_equal(
      "coerced params: operation", "other",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "operation")));
  nr_php_zval_free(&input);
  nr_php_zval_free(&output);

  input = tlib_php_request_eval_expr(
      "array('product' => 'p', 'collection' => 'c', 'operation' => 'o', 'host' "
      "=> 'h', 'portPathOrId' => 'pp', 'databaseName' => 'db', 'query' => "
      "'select', 'inputQueryLabel' => 'Doctrine', 'inputQuery' => "
      "'GET')" TSRMLS_CC);
  output = nr_php_api_datastore_validate(Z_ARRVAL_P(input));
  tlib_pass_if_not_null("all params", output);
  tlib_pass_if_zval_type_is("all params: type", IS_ARRAY, output);
  tlib_pass_if_size_t_equal("all params: size", 9,
                            nr_php_zend_hash_num_elements(Z_ARRVAL_P(output)));
  tlib_pass_if_str_equal(
      "all params: product", "p",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "product")));
  tlib_pass_if_str_equal(
      "all params: collection", "c",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "collection")));
  tlib_pass_if_str_equal(
      "all params: operation", "o",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "operation")));
  tlib_pass_if_str_equal(
      "all params: host", "h",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "host")));
  tlib_pass_if_str_equal(
      "all params: portPathOrId", "pp",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "portPathOrId")));
  tlib_pass_if_str_equal(
      "all params: databaseName", "db",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "databaseName")));
  tlib_pass_if_str_equal(
      "all params: query", "select",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "query")));
  tlib_pass_if_str_equal(
      "all params: inputQueryLabel", "Doctrine",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "inputQueryLabel")));
  tlib_pass_if_str_equal(
      "all params: inputQuery", "GET",
      Z_STRVAL_P(nr_php_zend_hash_find(Z_ARRVAL_P(output), "inputQuery")));
  nr_php_zval_free(&input);
  nr_php_zval_free(&output);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_create_instance_from_params(TSRMLS_C);
  test_validate(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
