/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument.h"
#include "util_hash.h"
#include "util_logging.h"
#define DECLARE_nr_php_wraprec_hashmap_API
#include "php_user_instrument_lookup.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if LOOKUP_METHOD == LOOKUP_USE_OP_ARRAY
static void test_op_array_wraprec(TSRMLS_D) {
  zend_op_array oparray = {.function_name = (void*)1};
  nruserfn_t func = {0};

  tlib_php_request_start();

  nr_php_op_array_set_wraprec(&oparray, &func TSRMLS_CC);
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC),
                         &func);

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  /*
   * Invalidate the cached pid.
   */
  NRPRG(pid) -= 1;

  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL);

  /*
   * Restore the cached pid and invalidate the mangled pid/index value.
   */
  NRPRG(pid) += 1;

  {
    unsigned long pval;

    pval = (unsigned long)oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];
    oparray.reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)(pval * 2);
  }

  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_op_array_get_wraprec(&oparray TSRMLS_CC), NULL);
#endif /* PHP >= 7.3 */

  tlib_php_request_end();
}
#endif /* PHP < 7.4 */

#if LOOKUP_METHOD == LOOKUP_USE_UTIL_HASHMAP || LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP

#define STRING_SZ(s) (s), nr_strlen(s)

static void mock_zend_function(zend_function* zf,
                               zend_uchar type,
                               const char* file_name,
                               const uint32_t line_no,
                               const char* func_name) {
  zf->type = type;
  zf->common.function_name = zend_string_init(STRING_SZ(func_name), 0);
  if (type == ZEND_USER_FUNCTION && NULL != file_name) {
    zf->op_array.filename = zend_string_init(STRING_SZ(file_name), 0);
    zf->op_array.line_start = line_no;
  }
}

static void mock_internal_function(zend_function* zf, const char* func_name) {
  mock_zend_function(zf, ZEND_INTERNAL_FUNCTION, NULL, 0, func_name);
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
  zf->common.scope = ce;
}

static void mock_user_closure(zend_function* zf,
                              const char* file_name,
                              const uint32_t line_no) {
  mock_user_function(zf, file_name, line_no, "{closure}");
  zf->common.fn_flags |= ZEND_ACC_CLOSURE;
}

static void mock_zend_function_destroy(zend_function* zf) {
  zend_string_release(zf->common.function_name);
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
#endif

#if LOOKUP_METHOD == LOOKUP_USE_UTIL_HASHMAP
static void test_user_instrument_hashmap() {
  char* key;
  size_t key_len;
#define FILE_NAME "/some/random/path/to/a_file.php"
#define LINE_NO 10
#define SCOPE_NAME "a_scope"
#define FUNC_NAME "a_function"

  zend_function user_closure = {0};
  zend_function user_function = {0};
  zend_function user_function_with_scope = {0};
  zend_function internal_function = {0};

  mock_user_closure(&user_closure, FILE_NAME, LINE_NO);
  mock_user_function(&user_function, FILE_NAME, LINE_NO, FUNC_NAME);
  mock_user_function_with_scope(&user_function_with_scope, FILE_NAME, LINE_NO,
                                SCOPE_NAME, FUNC_NAME);
  mock_internal_function(&internal_function, FUNC_NAME);

  /* Do asserts work? (don't blow up on invalid input) */
  tlib_pass_if_null("NULL args for z2fkey", zf2key(NULL, NULL));
  tlib_pass_if_null("NULL key_len for z2fkey", zf2key(NULL, &user_function));
  tlib_pass_if_null("NULL zend_function for z2fkey", zf2key(&key_len, NULL));
  tlib_pass_if_null("zf2key must not work for internal functions",
                    zf2key(&key_len, &internal_function));

  /* Happy path */
  key = zf2key(&key_len, &user_function);
  tlib_pass_if_not_null("key was generated for user_function", key);
  printf("user function key = %s\n", key);
  key = zf2key(&key_len, &user_closure);
  tlib_pass_if_not_null("key was generated for user_closure", key);
  printf("user closure key = %s\n", key);

  mock_zend_function_destroy(&user_closure);
  mock_zend_function_destroy(&user_function);
  mock_zend_function_destroy(&user_function_with_scope);
  mock_zend_function_destroy(&internal_function);
}
#endif

#if LOOKUP_METHOD == LOOKUP_USE_LINKED_LIST
static void test_get_wraprec_by_func() {
  return;
  zend_function zend_func = {0};
  zend_string* scope_name = NULL;
  zend_class_entry ce = {0};
  nruserfn_t* wraprec = NULL;
  char* name_str = "my_func_name";
  char* file_str = "my_file_name";

  tlib_php_request_start();

  /*
   * NULL if there's no wraprecs in the internal list.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);

  wraprec = nr_php_add_custom_tracer_named(
      "ClassNoMatch::functionNoMatch",
      nr_strlen("ClassNoMatch::functionNoMatch"));
  wraprec = nr_php_add_custom_tracer_named("functionNoMatch2",
                                           nr_strlen("functionNoMatch2"));
  wraprec = nr_php_add_custom_tracer_named(name_str, nr_strlen(name_str));

  /*
   * NULL if zend_function is NULL.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(NULL), NULL);

  /*
   * NULL if function name is NULL.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);

  /*
   * NULL if name is correct but type is wrong.
   */
  zend_func.type = ZEND_INTERNAL_FUNCTION;
  zend_func.common.function_name
      = zend_string_init(name_str, strlen(name_str), 0);
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);
  /*
   * Valid if function name matches and type is user function.
   */
  zend_func.type = ZEND_USER_FUNCTION;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);
  /*
   * NULL if function name matches, but class name doesn't.
   */
  scope_name = zend_string_init("ClassName", strlen("ClassName"), 0);
  ce.name = scope_name;
  zend_func.common.scope = &ce;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);
  /*
   * NULL if function name doesn't match and class name matches.
   */

  wraprec = nr_php_add_custom_tracer_named(
      "ClassName::my_func_name2", nr_strlen("ClassName::my_func_name2"));
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);
  /*
   * Valid if function name matches and class name matches.
   */
  wraprec = nr_php_add_custom_tracer_named(
      "ClassName::my_func_name", nr_strlen("ClassName::my_func_name"));
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);

  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);
  /*
   * Not NULL because we don't care about the reserved array anymore.
   */
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);

  /*
   * Valid if lineno/filename match.
   */

  /*
   * Because no lineno/filename are set yet, this wraprec matches using
   * funcname/classname.
   */
  wraprec = nr_php_get_wraprec_by_func(&zend_func);
  /*
   * Let's add filename/lineno to func.  Because it doesn't exist in wraprec, it
   * should match with funcname/classname, but should then ADD the
   * filename/lineno to the wraprec.
   */
  zend_func.op_array.filename = zend_string_init(file_str, strlen(file_str), 0);
  zend_func.op_array.line_start = 4;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);
  tlib_pass_if_ptr_equal(
      "obtain instrumented function filename added after wraprec is created",
      nr_php_get_wraprec_by_func(&zend_func), wraprec);
  tlib_pass_if_int_equal(
      "obtain instrumented function lineno added after wraprec is created", 4,
      wraprec->lineno);
  /*
   * Now if we remove funcname and klassname, should still match by
   * lineno/filename that was added in previous test.
   */

  zend_string_release(zend_func.common.function_name);
  zend_func.common.function_name = NULL;
  zend_string_release(zend_func.common.scope->name);
  zend_func.common.scope->name = NULL;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), wraprec);

  /*
   * NULL if filename matches but lineno doesn't.
   */
  zend_func.op_array.line_start = 41;
  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);

  /*
   * NULL if lineno matches but filename doesn't.
   */
  zend_func.op_array.line_start = 4;
  zend_string_release(zend_func.op_array.filename);
  zend_func.op_array.filename = NULL;

  tlib_pass_if_ptr_equal("obtain instrumented function",
                         nr_php_get_wraprec_by_func(&zend_func), NULL);

  tlib_php_request_end();
}
#endif

#if LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP
static void test_wraprec_hashmap() {
#define FILE_NAME "/some/random/path/to/a_file.php"
#define LINE_NO 10
#define SCOPE_NAME "a_scope"
#define FUNC_NAME "a_function"

  zend_function user_closure = {0};
  zend_function user_function = {0};
  zend_function user_function_with_scope = {0};
  zend_function internal_function = {0};

  mock_user_closure(&user_closure, FILE_NAME, LINE_NO);
  mock_user_function(&user_function, FILE_NAME, LINE_NO, FUNC_NAME);
  mock_user_function_with_scope(&user_function_with_scope, FILE_NAME, LINE_NO,
                                SCOPE_NAME, FUNC_NAME);
  mock_internal_function(&internal_function, FUNC_NAME);

  /* Do asserts work? (don't blow up on invalid input) */

  /* Happy path */
  // tlib_pass_if_not_null("key was generated for user_function", );
  // tlib_pass_if_not_null("key was generated for user_closure", key);

  mock_zend_function_destroy(&user_closure);
  mock_zend_function_destroy(&user_function);
  mock_zend_function_destroy(&user_function_with_scope);
  mock_zend_function_destroy(&internal_function);
}
#endif

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  // test_user_instrument_set();
  // test_user_instrument_get();

#if LOOKUP_METHOD == LOOKUP_USE_OP_ARRAY
  test_op_array_wraprec(TSRMLS_C);
#elif LOOKUP_METHOD == LOOKUP_USE_LINKED_LIST
  test_get_wraprec_by_func();
#elif LOOKUP_METHOD == LOOKUP_USE_UTIL_HASHMAP
  test_user_instrument_hashmap();
#elif LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP
  test_wraprec_hashmap();
#else

#error "Unknown wraprec lookup method"

#endif

  tlib_php_engine_destroy(TSRMLS_C);
}
