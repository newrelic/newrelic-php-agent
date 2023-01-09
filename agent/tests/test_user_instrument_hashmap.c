/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument_hashmap.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO

#define STRING_SZ(s) (s), nr_strlen(s)

static void mock_zend_function(zend_function* zf,
                               zend_uchar type,
                               const char* file_name,
                               const uint32_t line_no,
                               const char* func_name) {
  zf->type = type;
  zf->op_array.function_name = zend_string_init(STRING_SZ(func_name), 0);
  zend_string_hash_func(zf->op_array.function_name);
  if (type == ZEND_USER_FUNCTION && NULL != file_name) {
    zf->op_array.filename = zend_string_init(STRING_SZ(file_name), 0);
    zend_string_hash_func(zf->op_array.filename);
    zf->op_array.line_start = line_no;
  }
}

static void mock_user_function(zend_function* zf,
                               const char* file_name,
                               const uint32_t line_no,
                               const char* func_name) {
  mock_zend_function(zf, ZEND_USER_FUNCTION, file_name, line_no, func_name);
}

static void mock_user_function_with_scope(zend_function* zf,
                                          const char* file_name,
                                          const uint32_t line_no,
                                          const char* scope_name,
                                          const char* func_name) {
  mock_user_function(zf, file_name, line_no, func_name);

  zend_class_entry* ce = nr_calloc(1, sizeof(zend_class_entry));
  ce->name = zend_string_init(STRING_SZ(scope_name), 0);
  zend_string_hash_func(ce->name);
  zf->common.scope = ce;
}

static void mock_user_closure(zend_function* zf,
                              const char* file_name,
                              const uint32_t line_no) {
  mock_user_function(zf, file_name, line_no, "{closure}");
  zf->common.fn_flags |= ZEND_ACC_CLOSURE;
}

static void mock_zend_function_destroy(zend_function* zf) {
  zend_string_release(zf->op_array.function_name);
  if (zf->op_array.filename) {
    zend_string_release(zf->op_array.filename);
  }
  if (zf->common.scope) {
    if (zf->common.scope->name) {
      zend_string_release(zf->common.scope->name);
    }
    nr_realfree((void**)&zf->common.scope);
  }
}

static void reset_wraprec(nruserfn_t* w) {
  nr_php_wraprec_hashmap_key_release(&w->key);
}

static void test_wraprecs_hashmap() {
#define FILE_NAME "/some/random/path/to/a_file.php"
#define LINENO_BASE 10
#define SCOPE_NAME "a_scope"
#define FUNC_NAME "a_function"

  zend_function user_closure = {0};
  zend_function user_function = {0};
  zend_function user_function_with_scope = {0};
  zend_function zend_function_copy = {0};
  nruserfn_t wr1 = {0}, wr2 = {0}, wr3 = {0};
  int rc = 0;
  nruserfn_t* wraprec_found = NULL;
  nr_php_wraprec_hashmap_t* h = NULL;

  mock_user_closure(&user_closure, FILE_NAME, LINENO_BASE);
  mock_user_function(&user_function, FILE_NAME, LINENO_BASE + 1, FUNC_NAME);
  mock_user_function_with_scope(&user_function_with_scope, FILE_NAME,
                                LINENO_BASE + 2, SCOPE_NAME, FUNC_NAME);

  h = nr_php_wraprec_hashmap_create_buckets(16, reset_wraprec);
  tlib_fail_if_null("hashmap created", h);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function, &wraprec_found);
  tlib_pass_if_int_equal("can't anything in an empty hashmap", 0, rc);
  tlib_pass_if_null("can't find anything in an empty hashmap", wraprec_found);

  nr_php_wraprec_hashmap_update(h, &user_closure, &wr1);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_closure.op_array.line_start, wr1.key.lineno);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set function name",
      wr1.key.function_name);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set scope name",
      wr1.key.scope_name);
  tlib_pass_if_not_null("adding wraprec for unnamed function sets file name",
                        wr1.key.filename);
  tlib_pass_if_str_equal("adding wraprec for unnamed function sets file name",
                         FILE_NAME, ZSTR_VAL(wr1.key.filename));

  nr_php_wraprec_hashmap_update(h, &user_function, &wr2);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function.op_array.line_start,
                              wr2.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/o scope sets function name",
      wr2.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/o scope sets function name",
      FUNC_NAME, ZSTR_VAL(wr2.key.function_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set scope name",
      wr2.key.scope_name);
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set file name",
      wr2.key.filename);

  nr_php_wraprec_hashmap_update(h, &user_function_with_scope, &wr3);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function_with_scope.op_array.line_start,
                              wr3.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets function name",
      wr3.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets function name", FUNC_NAME,
      ZSTR_VAL(wr3.key.function_name));
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets scope name",
      wr3.key.scope_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets scope name", SCOPE_NAME,
      ZSTR_VAL(wr3.key.scope_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/scope does not set file name",
      wr3.key.filename);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_closure, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope", &wr1,
                         wraprec_found);

  zend_function_copy = user_closure;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr1,
      wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope", &wr2,
                         wraprec_found);

  zend_function_copy = user_function;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr2,
      wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function_with_scope,
                                       &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/scope", &wr3,
                         wraprec_found);

  zend_function_copy = user_function_with_scope;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/scope by zend_function copy", &wr3,
      wraprec_found);

  nr_php_wraprec_hashmap_destroy(&h);

  mock_zend_function_destroy(&user_closure);
  mock_zend_function_destroy(&user_function);
  mock_zend_function_destroy(&user_function_with_scope);
}

static void test_zend_string_hash_before_set() {
#define FILE_NAME "/some/random/path/to/a_file.php"
#define LINENO_BASE 10
#define SCOPE_NAME "a_scope"
#define FUNC_NAME "a_function"

  zend_function user_closure = {0};
  zend_function user_function = {0};
  zend_function user_function_with_scope = {0};
  zend_function zend_function_copy = {0};
  nruserfn_t wr1 = {0}, wr2 = {0}, wr3 = {0};
  int rc = 0;
  nruserfn_t* wraprec_found = NULL;
  nr_php_wraprec_hashmap_t* h = NULL;
  uint32_t hash;

  mock_user_closure(&user_closure, FILE_NAME, LINENO_BASE);
  mock_user_function(&user_function, FILE_NAME, LINENO_BASE + 1, FUNC_NAME);
  mock_user_function_with_scope(&user_function_with_scope, FILE_NAME,
                                LINENO_BASE + 2, SCOPE_NAME, FUNC_NAME);

  h = nr_php_wraprec_hashmap_create_buckets(16, reset_wraprec);
  tlib_fail_if_null("hashmap created", h);

  hash = ZSTR_H(user_closure.op_array.filename);
  ZSTR_H(user_closure.op_array.filename) = 0;
  nr_php_wraprec_hashmap_update(h, &user_closure, &wr1);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_closure.op_array.line_start, wr1.key.lineno);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set function name",
      wr1.key.function_name);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set scope name",
      wr1.key.scope_name);
  tlib_pass_if_not_null("adding wraprec for unnamed function sets file name",
                        wr1.key.filename);
  tlib_pass_if_str_equal("adding wraprec for unnamed function sets file name",
                         FILE_NAME, ZSTR_VAL(wr1.key.filename));
  tlib_pass_if_uint32_t_equal(
      "adding wraprec for unnamed function sets file name's hash", hash,
      ZSTR_H(wr1.key.filename));

  hash = ZSTR_H(user_function.op_array.function_name);
  ZSTR_H(user_function.op_array.function_name) = 0;
  nr_php_wraprec_hashmap_update(h, &user_function, &wr2);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function.op_array.line_start,
                              wr2.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/o scope sets function name",
      wr2.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/o scope sets function name",
      FUNC_NAME, ZSTR_VAL(wr2.key.function_name));
  tlib_pass_if_uint32_t_equal(
      "adding wraprec for named function w/o scope sets function name's hash",
      hash, ZSTR_H(wr2.key.function_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set scope name",
      wr2.key.scope_name);
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set file name",
      wr2.key.filename);

  hash = ZSTR_H(user_function_with_scope.op_array.function_name);
  ZSTR_H(user_function_with_scope.op_array.function_name) = 0;
  nr_php_wraprec_hashmap_update(h, &user_function_with_scope, &wr3);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function_with_scope.op_array.line_start,
                              wr3.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets function name",
      wr3.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets function name", FUNC_NAME,
      ZSTR_VAL(wr3.key.function_name));
  tlib_pass_if_uint32_t_equal(
      "adding wraprec for named function w/scope sets function name's hash",
      hash, ZSTR_H(wr2.key.function_name));
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets scope name",
      wr3.key.scope_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets scope name", SCOPE_NAME,
      ZSTR_VAL(wr3.key.scope_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/scope does not set file name",
      wr3.key.filename);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_closure, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope", &wr1,
                         wraprec_found);

  zend_function_copy = user_closure;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr1,
      wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope", &wr2,
                         wraprec_found);

  zend_function_copy = user_function;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr2,
      wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function_with_scope,
                                       &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/scope", 1, rc);
  tlib_pass_if_ptr_equal("can find named function w/scope", &wr3,
                         wraprec_found);

  zend_function_copy = user_function_with_scope;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/scope by zend_function copy", &wr3,
      wraprec_found);

  nr_php_wraprec_hashmap_destroy(&h);

  mock_zend_function_destroy(&user_closure);
  mock_zend_function_destroy(&user_function);
  mock_zend_function_destroy(&user_function_with_scope);
}

static void test_zend_string_hash_after_set_before_get() {
#define FILE_NAME "/some/random/path/to/a_file.php"
#define LINENO_BASE 10
#define SCOPE_NAME "a_scope"
#define FUNC_NAME "a_function"

  zend_function user_closure = {0};
  zend_function user_function = {0};
  zend_function user_function_with_scope = {0};
  zend_function zend_function_copy = {0};
  nruserfn_t wr1 = {0}, wr2 = {0}, wr3 = {0};
  int rc = 0;
  nruserfn_t* wraprec_found = NULL;
  nr_php_wraprec_hashmap_t* h = NULL;

  mock_user_closure(&user_closure, FILE_NAME, LINENO_BASE);
  mock_user_function(&user_function, FILE_NAME, LINENO_BASE + 1, FUNC_NAME);
  mock_user_function_with_scope(&user_function_with_scope, FILE_NAME,
                                LINENO_BASE + 2, SCOPE_NAME, FUNC_NAME);

  h = nr_php_wraprec_hashmap_create_buckets(16, reset_wraprec);
  tlib_fail_if_null("hashmap created", h);

  nr_php_wraprec_hashmap_update(h, &user_closure, &wr1);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_closure.op_array.line_start, wr1.key.lineno);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set function name",
      wr1.key.function_name);
  tlib_pass_if_null(
      "adding wraprec for unnamed function does not set scope name",
      wr1.key.scope_name);
  tlib_pass_if_not_null("adding wraprec for unnamed function sets file name",
                        wr1.key.filename);
  tlib_pass_if_str_equal("adding wraprec for unnamed function sets file name",
                         FILE_NAME, ZSTR_VAL(wr1.key.filename));

  nr_php_wraprec_hashmap_update(h, &user_function, &wr2);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function.op_array.line_start,
                              wr2.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/o scope sets function name",
      wr2.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/o scope sets function name",
      FUNC_NAME, ZSTR_VAL(wr2.key.function_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set scope name",
      wr2.key.scope_name);
  tlib_pass_if_null(
      "adding wraprec for named function w/o scope does not set file name",
      wr2.key.filename);

  nr_php_wraprec_hashmap_update(h, &user_function_with_scope, &wr3);
  tlib_pass_if_uint32_t_equal("adding wraprec to hashmap updates lineno",
                              user_function_with_scope.op_array.line_start,
                              wr3.key.lineno);
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets function name",
      wr3.key.function_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets function name", FUNC_NAME,
      ZSTR_VAL(wr3.key.function_name));
  tlib_pass_if_not_null(
      "adding wraprec for named function w/scope sets scope name",
      wr3.key.scope_name);
  tlib_pass_if_str_equal(
      "adding wraprec for named function w/scope sets scope name", SCOPE_NAME,
      ZSTR_VAL(wr3.key.scope_name));
  tlib_pass_if_null(
      "adding wraprec for named function w/scope does not set file name",
      wr3.key.filename);

  wraprec_found = NULL;
  ZSTR_H(user_closure.op_array.filename) = 0;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_closure, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope after hash reset",
                         1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope after hash reset",
                         &wr1, wraprec_found);

  zend_function_copy = user_closure;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr1,
      wraprec_found);

  wraprec_found = NULL;
  ZSTR_H(user_function.op_array.function_name) = 0;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function, &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/o scope after hash reset",
                         1, rc);
  tlib_pass_if_ptr_equal("can find named function w/o scope after hash reset",
                         &wr2, wraprec_found);

  zend_function_copy = user_function;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/o scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/o scope by zend_function copy", &wr2,
      wraprec_found);

  wraprec_found = NULL;
  ZSTR_H(user_function_with_scope.op_array.function_name) = 0;
  rc = nr_php_wraprec_hashmap_get_into(h, &user_function_with_scope,
                                       &wraprec_found);
  tlib_pass_if_int_equal("can find named function w/scope after hash reset", 1,
                         rc);
  tlib_pass_if_ptr_equal("can find named function w/scope after hash reset",
                         &wr3, wraprec_found);

  zend_function_copy = user_function_with_scope;
  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zend_function_copy, &wraprec_found);
  tlib_pass_if_int_equal(
      "can find named function w/scope by zend_function copy", 1, rc);
  tlib_pass_if_ptr_equal(
      "can find named function w/scope by zend_function copy", &wr3,
      wraprec_found);

  nr_php_wraprec_hashmap_destroy(&h);

  mock_zend_function_destroy(&user_closure);
  mock_zend_function_destroy(&user_function);
  mock_zend_function_destroy(&user_function_with_scope);
}

static void test_wraprec_hashmap_two_functions() {
#define FILE_1_NAME "/some/random/path/to/a_file.php"
#define FILE_2_NAME "/some/random/path/to/b_file.php"
#define LINENO_BASE 10
#define SCOPE_1_NAME "a_scope"
#define FUNC_1_NAME "a_function"
#define SCOPE_2_NAME "b_scope"
#define FUNC_2_NAME "b_function"

  zend_function zf1 = {0};
  zend_function zf2 = {0};
  nruserfn_t wr1 = {0}, wr2 = {0};
  int rc = 0;
  nruserfn_t* wraprec_found = NULL;
  nr_php_wraprec_hashmap_t* h = NULL;
  nr_php_wraprec_hashmap_stats_t s = {};

  h = nr_php_wraprec_hashmap_create_buckets(16, reset_wraprec);
  tlib_fail_if_null("hashmap created", h);

  /* A function */
  mock_user_function_with_scope(&zf1, FILE_1_NAME, LINENO_BASE, SCOPE_1_NAME,
                                FUNC_1_NAME);
  nr_php_wraprec_hashmap_update(h, &zf1, &wr1);

  /* A function with the same in the same file but different scope */
  mock_user_function_with_scope(&zf2, FILE_1_NAME, LINENO_BASE, SCOPE_2_NAME,
                                FUNC_1_NAME);
  nr_php_wraprec_hashmap_update(h, &zf2, &wr2);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zf1, &wraprec_found);
  tlib_pass_if_int_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      1, rc);
  tlib_pass_if_ptr_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      &wr1, wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zf2, &wraprec_found);
  tlib_pass_if_int_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      1, rc);
  tlib_pass_if_ptr_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      &wr2, wraprec_found);

  s = nr_php_wraprec_hashmap_destroy(&h);
  tlib_pass_if_size_t_equal("all elements are stored", 2, s.elements);

  mock_zend_function_destroy(&zf1);
  mock_zend_function_destroy(&zf2);

  /* -------------------------------------------------------------------  */

  h = nr_php_wraprec_hashmap_create_buckets(16, reset_wraprec);
  tlib_fail_if_null("hashmap created", h);

  /* A function */
  mock_user_function_with_scope(&zf1, FILE_1_NAME, LINENO_BASE, SCOPE_1_NAME,
                                FUNC_1_NAME);
  nr_php_wraprec_hashmap_update(h, &zf1, &wr1);

  /* A function with the same in different file with different scope */
  mock_user_function_with_scope(&zf2, FILE_2_NAME, LINENO_BASE, SCOPE_2_NAME,
                                FUNC_1_NAME);
  nr_php_wraprec_hashmap_update(h, &zf2, &wr2);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zf1, &wraprec_found);
  tlib_pass_if_int_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      1, rc);
  tlib_pass_if_ptr_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      &wr1, wraprec_found);

  wraprec_found = NULL;
  rc = nr_php_wraprec_hashmap_get_into(h, &zf2, &wraprec_found);
  tlib_pass_if_int_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      1, rc);
  tlib_pass_if_ptr_equal(
      "Two functions with the same in the same file but different scope are "
      "stored separetely",
      &wr2, wraprec_found);

  s = nr_php_wraprec_hashmap_destroy(&h);
  tlib_pass_if_size_t_equal("all elements are stored", 2, s.elements);

  mock_zend_function_destroy(&zf1);
  mock_zend_function_destroy(&zf2);
}

#endif

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  test_wraprecs_hashmap();
  test_zend_string_hash_before_set();
  test_zend_string_hash_after_set_before_get();
  test_wraprec_hashmap_two_functions();
#endif /* PHP >= 7.4 */

  tlib_php_engine_destroy(TSRMLS_C);
}
