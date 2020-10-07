/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TLIB_PHP_HDR
#define TLIB_PHP_HDR

#include <stdbool.h>

#include "tlib_php_includes.h"

#include "php_includes.h"

#include "tlib_main.h"
#include "util_buffer.h"

/*
 * Purpose : Create a Zend Engine.
 *
 * Params  : 1. Any additional INI settings. Each setting must be separated by
 *              a newline.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Note    : The TSRM parameter needs to be a pointer to the usual value, hence
 *           you'll need to use PTSRMLS_CC instead of TSRMLS_CC.
 */
extern nr_status_t tlib_php_engine_create(const char* extra_ini PTSRMLS_DC);

/*
 * Purpose : Destroy a Zend Engine.
 */
extern void tlib_php_engine_destroy(TSRMLS_D);

/*
 * Purpose : Start a request within a previously created Zend Engine.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, you should
 *           probably call tlib_php_engine_destroy() and bail.
 */
extern nr_status_t tlib_php_request_start_impl(const char* file,
                                               int line TSRMLS_DC);

#define tlib_php_request_start() \
  tlib_php_request_start_impl(__FILE__, __LINE__ TSRMLS_CC)

/*
 * Purpose : End a request within the current Zend Engine.
 */
extern void tlib_php_request_end_impl(const char* file, int line);

#define tlib_php_request_end() tlib_php_request_end_impl(__FILE__, __LINE__)

/*
 * Purpose : Check if a request is currently active.
 *
 * Returns : True if a request is active; false otherwise.
 */
extern bool tlib_php_request_is_active(void);

/*
 * Purpose : Evaluate the given PHP code in the current request.
 *
 * Params  : 1. The code to evaluate.
 *
 * Warning : If a request has not been started via tlib_php_request_start(),
 *           this function will abort() the running process, as trying to
 *           evaluate PHP code without a running request results in memory
 *           corruption.
 */
extern void tlib_php_request_eval(const char* code TSRMLS_DC);

/*
 * Purpose : Evaluate the given PHP expression in the current request and
 *           return its result.
 *
 * Params  : 1. The expression to evaluate.
 *
 * Returns : The resultant zval, which must be freed when no longer required.
 *
 * Warning : If a request has not been started via tlib_php_request_start(),
 *           this function will abort() the running process, as trying to
 *           evaluate PHP code without a running request results in memory
 *           corruption.
 *
 * Warning : Internally, the Zend Engine literally surrounds the expression
 *           with "return " and ";". If you provide "$a + $b", it will be
 *           transformed into "return $a + $b;".
 *
 *           What this means is that the expression MUST be a true expression:
 *           it cannot include a semi-colon or anything that can't be returned.
 */
extern zval* tlib_php_request_eval_expr(const char* code TSRMLS_DC);

/*
 * Purpose : Return the output buffer for the current request.
 */
extern nrbuf_t* tlib_php_request_output(void);

/*
 * Purpose : Require that an extension is loaded, and attempt to load it if it
 *           isn't compiled into PHP.
 *
 * Params  : 1. The extension name.
 *
 * Returns : Non-zero if the extension is available; zero otherwise.
 */
extern int tlib_php_require_extension(const char* extension TSRMLS_DC);

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
typedef void (*tlib_php_internal_function_handler_t)(
    INTERNAL_FUNCTION_PARAMETERS);
#else
typedef void(ZEND_FASTCALL* tlib_php_internal_function_handler_t)(
    INTERNAL_FUNCTION_PARAMETERS);
#endif /* PHP < 7.3 */

/*
 * Purpose : Replace an internal function.
 *
 * Params  : 1. The class name, if the function to be replaced is a method, or
 *              NULL if the function is not a method.
 *           2. The function name.
 *           3. The new handler.
 *
 * Returns : The old handler, or NULL on error.
 *
 * Warning : 1. This function MUST be called outside a request in ZTS mode.
 *              (Just always call it outside a request and save yourself the
 *              trouble.)
 *
 *           2. The only ways to reverse the effect of this function are to
 *              either destroy the PHP engine and create a new one, or to
 *              invoke this function again with the old handler. It is NOT
 *              scoped to the current request, as PHP does not fully recreate
 *              the executor's function table each request: internal functions
 *              are persisted.
 */
extern tlib_php_internal_function_handler_t tlib_php_replace_internal_function(
    const char* klass,
    const char* function,
    tlib_php_internal_function_handler_t handler TSRMLS_DC);

/*
 * Purpose : Create a zval of the given type with the default value for that
 *           type.
 *
 * Params  : 1. The zval type.
 *
 * Returns : A newly created zval, which the caller owns.
 *
 * Note    : The default value isn't always obvious, so here's what you get in
 *           all cases:
 *
 *           IS_NULL:      null
 *           IS_LONG:      0
 *           IS_DOUBLE:    0.0
 *           IS_ARRAY:     []
 *           IS_OBJECT:    new stdClass
 *           IS_STRING:    ""
 *           IS_RESOURCE:  resource(0) of type (unknown)
 *           IS_UNDEF:     UNKNOWN                        (PHP 7 only)
 *           IS_FALSE:     false                          (PHP 7 only)
 *           IS_TRUE:      true                           (PHP 7 only)
 *           IS_REFERENCE: empty reference                (PHP 7 only)
 *           IS_BOOL:      false                          (PHP 5 only)
 */
extern zval* tlib_php_zval_create_default(zend_uchar type TSRMLS_DC);

/*
 * Purpose : Utility function to provide an array of every zval type.
 *
 * Returns : An array of zval pointers.
 */
extern zval** tlib_php_zvals_of_all_types(TSRMLS_D);

/*
 * Purpose : Utility function to provide an array of every zval type except the
 *           one given. (Useful for testing internal functions that can only
 *           operate on one type.)
 *
 * Params  : 1. The type that should _NOT_ be included in the returned array.
 *              This type may only be IS_NULL, IS_LONG, IS_DOUBLE, IS_ARRAY,
 *              IS_OBJECT, IS_STRING or IS_RESOURCE, due to incompatibilities
 *              between PHP 5 and 7.
 *
 * Returns : An array of zval pointers.
 */
extern zval** tlib_php_zvals_not_of_type(zend_uchar type TSRMLS_DC);

/*
 * Purpose : Free an array returned by tlib_php_zvals_not_of_type() or
 *           tlib_php_zvals_of_all_types().
 *
 * Params  : 1. A pointer to the array to free.
 */
extern void tlib_php_free_zval_array(zval*** arr_ptr);

/*
 * Purpose : Utility function to generate a string representation of a zval
 *           using var_dump().
 *
 * Params  : 1. The zval to var_dump().
 *
 * Returns : A string, which the caller must free, or NULL on error.
 */
extern char* tlib_php_zval_dump(zval* zv TSRMLS_DC);

/*
 * Internal macros used for formatting messages.
 *
 * Some assertions below require multiple assertions underneath. Since the line
 * number will always be the caller's line number, it's not necessarily clear
 * which sub-assertion failed. These macros allow the convenience macros below
 * to format the message given by the caller to include which sub-assertion
 * failed.
 *
 * Basic usage: declare and allocate the heap variables it needs with
 * tlib_specific_message_alloc(), set the current message with
 * tlib_specific_message_set(), pass the message to other assertions with
 * tlib_specific_message(), and then free it with tlib_specific_message_free().
 */

/*
 * This macro actually declares (hopefully prefixed so that they'll never
 * clash) variables as well as allocating them, so the name sucks.
 */
#define tlib_specific_message_alloc(NAME, STEM, SUFFIXLEN)   \
  const char* tlib_test_message_##NAME##_stem = (STEM);      \
  size_t tlib_test_message_##NAME##_len                      \
      = nr_strlen(tlib_test_message_##NAME##_stem);          \
  size_t tlib_test_message_##NAME##_suffixlen = (SUFFIXLEN); \
  char* tlib_test_message_##NAME                             \
      = nr_zalloc(tlib_test_message_##NAME##_len             \
                  + tlib_test_message_##NAME##_suffixlen + 1);

#define tlib_specific_message_free(NAME) nr_free(tlib_test_message_##NAME)

#define tlib_specific_message_set(NAME, SUFFIX)                            \
  do {                                                                     \
    if ('\0' == *tlib_test_message_##NAME) {                               \
      nr_memcpy(tlib_test_message_##NAME, tlib_test_message_##NAME##_stem, \
                tlib_test_message_##NAME##_len);                           \
    }                                                                      \
                                                                           \
    nr_strlcpy(tlib_test_message_##NAME + tlib_test_message_##NAME##_len,  \
               (SUFFIX), tlib_test_message_##NAME##_suffixlen + 1);        \
  } while (0)

#define tlib_specific_message(NAME) tlib_test_message_##NAME

/*
 * This checks the class and function names of the given zend_function. If the
 * zend_function is not expected to be a method, pass NULL as the expected
 * class name.
 */
#define tlib_pass_if_zend_function_is(M, CLASSNAME, NAME, FUNC)      \
  do {                                                               \
    const char* tlib_test_classname = (CLASSNAME);                   \
    const zend_function* tlib_test_func = (FUNC);                    \
    tlib_specific_message_alloc(zfi, (M), sizeof(" function name")); \
                                                                     \
    tlib_specific_message_set(zfi, " scope");                        \
    if (tlib_test_classname) {                                       \
      tlib_fail_if_null(tlib_specific_message(zfi),                  \
                        tlib_test_func->common.scope);               \
                                                                     \
      tlib_specific_message_set(zfi, " class name");                 \
      tlib_pass_if_str_equal(                                        \
          tlib_specific_message(zfi), tlib_test_classname,           \
          nr_php_class_entry_name(tlib_test_func->common.scope));    \
    } else {                                                         \
      tlib_pass_if_null(tlib_specific_message(zfi),                  \
                        tlib_test_func->common.scope);               \
    }                                                                \
                                                                     \
    tlib_specific_message_set(zfi, " function name");                \
    tlib_pass_if_str_equal(tlib_specific_message(zfi), (NAME),       \
                           nr_php_function_name(func));              \
                                                                     \
    tlib_specific_message_free(zfi);                                 \
  } while (0)

#define tlib_pass_if_zval_type_is(M, EXPECTED, ZVAL) \
  tlib_pass_if_int_equal(M, EXPECTED, Z_TYPE_P(ZVAL))

/*
 * Booleans are tricky, since there are different zval types in PHP 5 and 7.
 * We'll define a couple of macros to make this easier.
 *
 * If you're only interested in whether a value is truthy, you should call
 * nr_php_is_zval_true() directly and assert on that.
 */
#define tlib_pass_if_zval_is_bool_value(M, EXPECTED, ZVAL)             \
  do {                                                                 \
    zval* tlib_test_zval = (ZVAL);                                     \
    tlib_specific_message_alloc(zib, (M), sizeof(" value"));           \
                                                                       \
    tlib_specific_message_set(zib, " type");                           \
    tlib_fail_if_int_equal(tlib_specific_message(zib), 0,              \
                           nr_php_is_zval_valid_bool(tlib_test_zval)); \
                                                                       \
    tlib_specific_message_set(zib, " value");                          \
    if (EXPECTED) {                                                    \
      tlib_fail_if_int_equal(tlib_specific_message(zib), 0,            \
                             nr_php_is_zval_true(tlib_test_zval));     \
    } else {                                                           \
      tlib_pass_if_int_equal(tlib_specific_message(zib), 0,            \
                             nr_php_is_zval_true(tlib_test_zval));     \
    }                                                                  \
                                                                       \
    tlib_specific_message_free(zib);                                   \
  } while (0)

#define tlib_pass_if_zval_is_bool_false(M, ZVAL) \
  tlib_pass_if_zval_is_bool_value((M), false, (ZVAL))
#define tlib_pass_if_zval_is_bool_true(M, ZVAL) \
  tlib_pass_if_zval_is_bool_value((M), true, (ZVAL))

extern void tlib_pass_if_zval_identical_f(const char* msg,
                                          zval* expected,
                                          zval* actual,
                                          const char* file,
                                          int line TSRMLS_DC);

#define tlib_pass_if_zval_identical(M, EXPECTED, ACTUAL)             \
  tlib_pass_if_zval_identical_f((M), (EXPECTED), (ACTUAL), __FILE__, \
                                __LINE__ TSRMLS_CC)

#endif /* TLIB_PHP_HDR */
