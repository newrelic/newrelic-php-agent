/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_output.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#define test_output_flag_func(MSG, FUNC, EXPECT, FLAGS)                   \
  do {                                                                    \
    int __act = FUNC(FLAGS);                                              \
    const char* __msg = (MSG);                                            \
                                                                          \
    if (EXPECT) {                                                         \
      tlib_pass_if_true(__msg, __act, "expected=true actual=%d", __act);  \
    } else {                                                              \
      tlib_pass_if_false(__msg, __act, "expected=true actual=%d", __act); \
    }                                                                     \
  } while (0)

static void test_output_flags(void) {
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_WRITE);
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("has content", nr_php_output_has_content, 0,
                        PHP_OUTPUT_HANDLER_CLEAN);
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_FLUSH);
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_FINAL);
  test_output_flag_func("has content", nr_php_output_has_content, 0, INT_MAX);

  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_WRITE);
  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_CLEAN);
  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_FLUSH);
  test_output_flag_func("is end", nr_php_output_is_end, 1,
                        PHP_OUTPUT_HANDLER_FINAL);
  test_output_flag_func("is end", nr_php_output_is_end, 1, INT_MAX);

  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_WRITE);
  test_output_flag_func("is start", nr_php_output_is_start, 1,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_CLEAN);
  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_FLUSH);
  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_FINAL);
  test_output_flag_func("is start", nr_php_output_is_start, 1, INT_MAX);
#else  /* PHP < 5.4 */
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_CONT);
  test_output_flag_func("has content", nr_php_output_has_content, 1,
                        PHP_OUTPUT_HANDLER_END);
  test_output_flag_func("has content", nr_php_output_has_content, 1, INT_MAX);

  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("is end", nr_php_output_is_end, 0,
                        PHP_OUTPUT_HANDLER_CONT);
  test_output_flag_func("is end", nr_php_output_is_end, 1,
                        PHP_OUTPUT_HANDLER_END);
  test_output_flag_func("is end", nr_php_output_is_end, 1, INT_MAX);

  test_output_flag_func("is start", nr_php_output_is_start, 1,
                        PHP_OUTPUT_HANDLER_START);
  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_CONT);
  test_output_flag_func("is start", nr_php_output_is_start, 0,
                        PHP_OUTPUT_HANDLER_END);
  test_output_flag_func("is start", nr_php_output_is_start, 1, INT_MAX);
#endif /* PHP >= 5.4 */
}

/*
 * A dummy output handler for the nr_php_output_install_handler() tests.
 */
static void output_handler(char* output NRUNUSED,
                           nr_output_buffer_string_len_t output_len NRUNUSED,
                           char** handled_output NRUNUSED,
                           nr_output_buffer_string_len_t* handled_output_len
                               NRUNUSED,
                           int mode NRUNUSED TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (handled_output && handled_output_len) {
    *handled_output = NULL;
    *handled_output_len = 0;
  }
}

/*
 * Implementation function and wrapper macro for a test that asserts a certain
 * number of active output handlers.
 */
static void test_output_handler_count_f(const char* message,
                                        size_t expected,
                                        const char* file,
                                        int line TSRMLS_DC) {
  size_t actual;
  zval* handlers = NULL;

  handlers = nr_php_call(NULL, "ob_list_handlers");
  if (0 == nr_php_is_zval_valid_array(handlers)) {
    tlib_pass_if_true_f(message, 0, file, line, "handlers are invalid",
                        "handlers=%p", handlers);
    goto end;
  }

  actual = nr_php_zend_hash_num_elements(Z_ARRVAL_P(handlers));
  tlib_pass_if_true_f(message, expected == actual, file, line,
                      "incorrect number of array elements",
                      "expected=%zu actual=%zu", expected, actual);

end:
  nr_php_zval_free(&handlers);
}

#define test_output_handler_count(M, X) \
  test_output_handler_count_f((M), (X), __FILE__, __LINE__ TSRMLS_CC)

/*
 * Implementation function and wrapper macros for tests that assert that a
 * particular named output handler is either active or inactive.
 */
static void test_output_handler_f(const char* message,
                                  const char* name,
                                  int expected,
                                  const char* file,
                                  int line TSRMLS_DC) {
  zval* handler = NULL;
  zval* handlers = NULL;

  handlers = nr_php_call(NULL, "ob_list_handlers");
  if (0 == nr_php_is_zval_valid_array(handlers)) {
    tlib_pass_if_true_f(message, 0, file, line, "handlers are invalid",
                        "handlers=%p", handlers);
    goto end;
  }

  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(handlers), handler) {
    if (0 == nr_php_is_zval_valid_string(handler)) {
      tlib_pass_if_true_f(message, 0, file, line, "invalid handler", "type=%d",
                          (int)Z_TYPE_P(handler));
      goto end;
    }

    if (0
        == nr_strncmp(name, Z_STRVAL_P(handler),
                      NRSAFELEN(Z_STRLEN_P(handler)))) {
      if (expected) {
        tlib_did_pass();
      } else {
        tlib_pass_if_true_f(message, 0, file, line, "found unexpected handler",
                            "name=%s", name);
      }

      goto end;
    }
  }
  ZEND_HASH_FOREACH_END();

  if (expected) {
    zval* dump;
    const char* dumpstr = "(null)";

    dump = nr_php_call(NULL, "var_dump", handlers);
    if (nr_php_is_zval_valid_string(dump)) {
      dumpstr = Z_STRVAL_P(dump);
    }

    tlib_pass_if_true_f(message, 0, file, line, "handler not found",
                        "handlers=%s name=%s", dumpstr, name);

    nr_php_zval_free(&dump);
  } else {
    tlib_did_pass();
  }

end:
  nr_php_zval_free(&handlers);
}

#define test_output_handler_exists(M, N) \
  test_output_handler_f(M, N, 1, __FILE__, __LINE__ TSRMLS_CC)

#define test_output_handler_does_not_exist(M, N) \
  test_output_handler_f(M, N, 0, __FILE__, __LINE__ TSRMLS_CC)

static void test_output_install_handler(TSRMLS_D) {
  tlib_php_request_start();

  /*
   * Test : Bad parameters.
   */
  nr_php_output_install_handler("foo", NULL TSRMLS_CC);
  test_output_handler_count("NULL handler", 0);
  test_output_handler_does_not_exist("NULL handler", "foo");

  nr_php_output_install_handler(NULL, output_handler TSRMLS_CC);
  test_output_handler_count("NULL name", 0);

  /*
   * Test : Normal operation.
   */
  nr_php_output_install_handler("foo", output_handler TSRMLS_CC);
  test_output_handler_count("handler", 1);
  test_output_handler_exists("handler", "foo");

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_output_flags();
  test_output_install_handler(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
