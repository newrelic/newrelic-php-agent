/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "fw_hooks.h"
#include "fw_drupal_common.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_single_extract_module_name_from_hook_and_hook_function(
    const char* hook_function_name,
    char* hook_name,
    char* expected_module_name) {
  char* module = 0;
  size_t module_len = 0;

  module_invoke_all_parse_module_and_hook_from_strings(
      &module, &module_len, hook_name, strlen(hook_name), hook_function_name,
      strlen(hook_function_name));

  tlib_pass_if_str_equal("Extracted Correct Module Name", module,
                         expected_module_name);
  nr_free(module);
}

static void test_module_name() {
  int i = 0;
  int number_of_fixtures = 0;

  // a set of three string sets.
  // fixtures[i][0] = The full PHP function name of the hook
  // fixtures[i][1] = The portion of the function name that's the hook name
  // fixtures[i][2] = The module name we expect to be extracted

  char* fixtures[][3]
      = {{"modulename_hookname", "hookname", "modulename"},
         {"foo_bar", "bar", "foo"},
         {"help_help", "help", "help"},
         {"locale_locale", "locale", "locale"},
         {"menu_menu", "menu", "menu"},
         {"ckeditor_skin_ckeditor_skin", "ckeditor_skin", "ckeditor_skin"},
         {"context_context", "context", "context"},
         {"views_form_views_form", "views_form", "views_form"},
         {"atlas_statistics_atlas_statistics", "atlas_statistics",
          "atlas_statistics"},
         {"atlas_statistics_atlas_stat", "atlas_stat", "atlas_statistics"}};

  number_of_fixtures = sizeof(fixtures) / sizeof(fixtures[0]);

  for (i = 0; i < number_of_fixtures; i++) {
    test_single_extract_module_name_from_hook_and_hook_function(
        fixtures[i][0], fixtures[i][1], fixtures[i][2]);
  }
}

static void test_drupal_headers_add(TSRMLS_D) {
  zval* arg;
  zval* headers;
  zval* element;

  tlib_php_request_start();

  /* Drupal 7, NULL options */
  arg = nr_php_zval_alloc();
  ZVAL_NULL(arg);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: null returned for null passed",
                    nr_php_is_zval_null(arg), "type=%d", Z_TYPE_P(arg));

  nr_php_zval_free(&arg);

  /* Drupal 7, invalid options */
  arg = nr_php_zval_alloc();
  ZVAL_BOOL(arg, true);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: bool returned for bool passed",
                    nr_php_is_zval_valid_bool(arg), "type=%d", Z_TYPE_P(arg));

  nr_php_zval_free(&arg);

  /* Drupal 7, empty options */
  arg = nr_php_zval_alloc();
  array_init(arg);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: headers added for [] passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");

  tlib_pass_if_true("Drupal 7: headers added for [] passed",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers added are an array",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers array added is not empty",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)) > 0,
                    "len=%zu",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)));

  nr_php_zval_free(&arg);

  /* Drupal 7, invalid headers */
  arg = nr_php_zval_alloc();
  array_init(arg);

  headers = nr_php_zval_alloc();
  ZVAL_BOOL(headers, true);
  nr_php_add_assoc_zval(arg, "headers", headers);
  nr_php_zval_free(&headers);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: headers present for invalid headers added",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");

  tlib_pass_if_true(
      "Drupal 7: invalid headers present for invalid headers added",
      nr_php_is_zval_valid_bool(headers), "type=%d", Z_TYPE_P(headers));

  nr_php_zval_free(&arg);

  /* Drupal 7, empty headers */
  arg = nr_php_zval_alloc();
  array_init(arg);

  headers = nr_php_zval_alloc();
  array_init(headers);
  nr_php_add_assoc_zval(arg, "headers", headers);
  nr_php_zval_free(&headers);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: headers added for empty headers passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");

  tlib_pass_if_true("Drupal 7: headers added for empty headers passed",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers added are an array",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers array added is not empty",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)) > 0,
                    "len=%zu",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)));

  nr_php_zval_free(&arg);

  /* Drupal 7, non-empty headers */
  arg = nr_php_zval_alloc();
  array_init(arg);

  headers = nr_php_zval_alloc();
  array_init(headers);
  nr_php_add_assoc_string(headers, "a", "b");
  nr_php_add_assoc_zval(arg, "headers", headers);
  nr_php_zval_free(&headers);

  nr_drupal_headers_add(arg, true TSRMLS_CC);

  tlib_pass_if_true("Drupal 7: headers added for non-empty headers passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");

  tlib_pass_if_true("Drupal 7: headers added for non-empty headers passed",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers added are an array",
                    nr_php_is_zval_valid_array(headers), "type=%d",
                    Z_TYPE_P(headers));

  tlib_pass_if_true("Drupal 7: headers array has additional elements",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)) > 1,
                    "len=%zu",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)));

  element = nr_php_zend_hash_find(Z_ARRVAL_P(headers), "a");
  tlib_pass_if_true("Drupal 7: headers array has original element",
                    NULL != element, "type=%p", element);

  nr_php_zval_free(&arg);

  /* Drupal 6, NULL headers */
  arg = nr_php_zval_alloc();
  ZVAL_NULL(arg);

  nr_drupal_headers_add(arg, false TSRMLS_CC);

  tlib_pass_if_true("Drupal 6: empty array returned for null passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  nr_php_zval_free(&arg);

  /* Drupal 6, invalid headers */
  arg = nr_php_zval_alloc();
  ZVAL_BOOL(arg, true);

  nr_drupal_headers_add(arg, false TSRMLS_CC);

  tlib_pass_if_true("Drupal 6: bool returned for bool passed",
                    nr_php_is_zval_valid_bool(arg), "type=%d", Z_TYPE_P(arg));

  nr_php_zval_free(&arg);

  /* Drupal 6, empty headers */
  arg = nr_php_zval_alloc();
  array_init(arg);

  nr_drupal_headers_add(arg, false TSRMLS_CC);

  tlib_pass_if_true("Drupal 6: headers added for empty array passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  tlib_pass_if_true("Drupal 6: headers array added is not empty",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(arg)) > 0,
                    "len=%zu", nr_php_zend_hash_num_elements(Z_ARRVAL_P(arg)));

  nr_php_zval_free(&arg);

  /* Drupal 6, non-empty headers */
  arg = nr_php_zval_alloc();
  array_init(arg);

  nr_php_add_assoc_string(arg, "a", "b");

  nr_drupal_headers_add(arg, false TSRMLS_CC);

  tlib_pass_if_true("Drupal 6: headers added for non-empty headers passed",
                    nr_php_is_zval_valid_array(arg), "type=%d", Z_TYPE_P(arg));

  tlib_pass_if_true("Drupal 6: headers array has additional elements",
                    nr_php_zend_hash_num_elements(Z_ARRVAL_P(arg)) > 1,
                    "len=%zu", nr_php_zend_hash_num_elements(Z_ARRVAL_P(arg)));

  element = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "a");
  tlib_pass_if_true("Drupal 6: headers array has original element",
                    nr_php_is_zval_valid_string(element), "type=%d",
                    Z_TYPE_P(element));

  nr_php_zval_free(&arg);

  tlib_php_request_end();
}

static void test_drupal_http_request_drupal_7(TSRMLS_D) {
  zval* expr = NULL;
  zval* headers = NULL;
  zval* urlparam = NULL;
  zval* hdrparam = NULL;

  const char* valid_calls[] = {NULL, "array()", "array('headers' => array())",
                               "array('headers' => array('a' => 'b'))"};

  tlib_php_request_start();

  /*
   * drupal_http_request is mocked and then wrapped.
   */
  tlib_php_request_eval(
      "function drupal_http_request($url, $options = array()) {"
      "  return $options;"
      "}" TSRMLS_CC);
  nr_drupal_enable(TSRMLS_C);

  urlparam = tlib_php_request_eval_expr("'url'" TSRMLS_CC);

  /*
   * A list of valid arguments for $options is tested.
   */
  for (size_t i = 0; i < sizeof(valid_calls) / sizeof(valid_calls[0]); i++) {
    if (NULL != valid_calls[i]) {
      hdrparam = tlib_php_request_eval_expr(valid_calls[i] TSRMLS_CC);
      expr = nr_php_call(NULL, "drupal_http_request", urlparam, hdrparam);
    } else {
      expr = nr_php_call(NULL, "drupal_http_request", urlparam);
    }

    tlib_pass_if_not_null("Drupal 7: options is an array", expr);
    tlib_pass_if_zval_type_is("Drupal 7: options is an array", IS_ARRAY, expr);

    headers = nr_php_zend_hash_find(Z_ARRVAL_P(expr), "headers");

    tlib_pass_if_not_null("Drupal 7: headers is an array", headers);
    tlib_pass_if_zval_type_is("Drupal 7: headers is an array", IS_ARRAY,
                              headers);

    tlib_fail_if_size_t_equal(
        "Drupal 7: headers array is not empty", 0,
        nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers)));

    nr_php_zval_free(&hdrparam);
    nr_php_zval_free(&expr);
  }

  nr_php_zval_free(&urlparam);

  tlib_php_request_end();
}

static void test_drupal_http_request_drupal_6(TSRMLS_D) {
  zval* expr = NULL;
  zval* urlparam = NULL;
  zval* hdrparam = NULL;

  const char* valid_calls[] = {NULL, "array()", "array('a' => 'b')"};

  tlib_php_request_start();

  /*
   * drupal_http_request is mocked and then wrapped.
   */
  tlib_php_request_eval(
      "function drupal_http_request($url, $headers = array(), $method = 'GET',"
      "                             $data = NULL, $retry = 3,"
      "			            $timeout = 30.0) {"
      "  return $headers;"
      "}" TSRMLS_CC);
  nr_drupal_enable(TSRMLS_C);

  urlparam = tlib_php_request_eval_expr("'url'" TSRMLS_CC);

  /*
   * A list of valid arguments for $headers is tested.
   */
  for (size_t i = 0; i < sizeof(valid_calls) / sizeof(valid_calls[0]); i++) {
    if (NULL != valid_calls[i]) {
      hdrparam = tlib_php_request_eval_expr(valid_calls[i] TSRMLS_CC);
      expr = nr_php_call(NULL, "drupal_http_request", urlparam, hdrparam);
    } else {
      expr = nr_php_call(NULL, "drupal_http_request", urlparam);
    }

    tlib_pass_if_not_null("Drupal 6: headers is an array", expr);
    tlib_pass_if_zval_type_is("Drupal 6: headers is an array", IS_ARRAY, expr);

    tlib_fail_if_size_t_equal("Drupal 6: headers array is not empty", 0,
                              nr_php_zend_hash_num_elements(Z_ARRVAL_P(expr)));

    nr_php_zval_free(&hdrparam);
    nr_php_zval_free(&expr);
  }

  nr_php_zval_free(&urlparam);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */
  tlib_php_engine_create("" PTSRMLS_CC);
  test_module_name();
  test_drupal_headers_add(TSRMLS_C);
  test_drupal_http_request_drupal_7(TSRMLS_C);
  test_drupal_http_request_drupal_6(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}
