/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains PHP Execution hooks.
 */
#ifndef PHP_EXECUTE_HDR
#define PHP_EXECUTE_HDR

#include "php_observer.h"
#include "util_logging.h"

/*
 * An op_array is for a file rather than a function if it has a file name
 * and no function name.
 */
#define OP_ARRAY_IS_A_FILE(OP) \
  ((NULL == nr_php_op_array_function_name(OP)) && nr_php_op_array_file_name(OP))
#define OP_ARRAY_IS_A_FUNCTION(OP) \
  (nr_php_op_array_function_name(OP) && (0 == (OP)->scope))
#define OP_ARRAY_IS_FUNCTION(OP, FNAME) \
  (0 == nr_strcmp(nr_php_op_array_function_name(OP), (FNAME)))
#define OP_ARRAY_IS_A_METHOD(OP) \
  (nr_php_op_array_function_name(OP) && (OP)->scope)
#define OP_ARRAY_IS_METHOD(OP, FNAME) \
  (0 == nr_strcmp(nr_php_op_array_function_name(OP), (FNAME)))

/*
 * Purpose: Look through the PHP symbol table for special names or symbols
 * that provide additional hints that a specific framework has been loaded.
 *
 * Returns: a nr_framework_classification
 */
typedef enum {
  FRAMEWORK_IS_NORMAL,  /* the framework isn't special, but is treated normally
                         */
  FRAMEWORK_IS_SPECIAL, /* the framework is special */
} nr_framework_classification_t;
typedef nr_framework_classification_t (*nr_framework_special_fn_t)(
    const char* filename TSRMLS_DC);

/*
 * Version specific metadata that we have to gather before we call the original
 * execute_ex handler, as different versions of PHP behave differently in terms
 * of what you can do with the op array after making that call. This structure
 * and the functions immediately below are helpers for
 * nr_php_execute_enabled(), which is the user function execution function.
 *
 * In PHP 7, it is possible that the op array will be destroyed if the function
 * being called is a __call() magic method (in which case a trampoline is
 * created and destroyed). We increment the reference counts on the scope and
 * function strings and keep pointers to them in this structure, then release
 * them once we've named the trace node and/or metric (if required).
 *
 * In PHP 5, execute_data->op_array may be set to NULL if we make a subsequent
 * user function call in an exec callback (which occurs before we decide
 * whether to create a metric and/or trace node), so we keep a copy of the
 * pointer here. The op array itself won't be destroyed from under us, as it's
 * owned by the request and not the specific function call (unlike the
 * trampoline case above).
 *
 * Note that, while op arrays are theoretically reference counted themselves,
 * we cannot take the simple approach of incrementing that reference count due
 * to not all Zend Engine functions using init_op_array() and
 * destroy_op_array(): one example is that PHP 7 trampoline op arrays are
 * simply emalloc() and efree()'d without even setting the reference count.
 * Therefore we have to be more selective in our approach.
 */
typedef struct {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP7+ */
  zend_string* scope;
  zend_string* function;
  zend_string* filepath;
  uint32_t function_lineno;
  zval* execute_data_this;
#else
  zend_op_array* op_array;
#endif /* PHP7 */
} nr_php_execute_metadata_t;

extern nrframework_t nr_php_framework_from_config(const char* config_name);

/*
 * Purpose : Create a supportability metric with the name of the framework if a
 *           framework has been forced or detected.  This metric is used in the
 *           collector to set an application environment attribute.  This
 *           attribute may be used to enable certain features or pages.
 *
 *           This should be called at the end of each transaction during the
 *           request, not simply the transaction during which the framework was
 *           identified to ensure the attribute is attached to all applications
 *           which were a part of this request. For example:
 *           Suppose set_appname is called after Drupal is detected, we want
 *           the subsequent application to be also tagged as Drupal.
 */
extern void nr_framework_create_metric(TSRMLS_D);

/*
 * Purpose : Detect library and framework usage from the opcache status
 *
 *           This function gets a list of all files loaded into opcache and
 *           detects frameworks and libraries based on those files.
 *
 *           This is necessary to correctly instrument frameworks and libraries
 *           that are preloaded.
 */

extern void nr_php_user_instrumentation_from_opcache(TSRMLS_D);

extern void nr_php_observer_handle_uncaught_exception(zval* exception_this);

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP8+ */
static inline void php_observer_clear_uncaught_exception_globals() {
  /*
   * Clear the uncaught exception global variables.
   */
  if (NULL != NRPRG(uncaught_exception)) {
    nr_php_zval_free(&NRPRG(uncaught_exception));
  }
  NRPRG(uncaught_exeption_execute_data_this) = NULL;
}

static inline void php_observer_set_uncaught_exception_globals(
    zval* exception,
    zval* exception_this) {
  /*
   * Set the uncaught exception global variables
   */
  if (nrunlikely(NULL != NRPRG(uncaught_exception))) {
    return;
  }
  NRPRG(uncaught_exception) = nr_php_zval_alloc();
  ZVAL_DUP(NRPRG(uncaught_exception), exception);
  NRPRG(uncaught_exeption_execute_data_this) = exception_this;
}

/*
 * Purpose : Release any cached metadata.
 *
 * Params  : 1. A pointer to the metadata.
 */
extern void nr_php_execute_metadata_release(
    nr_php_execute_metadata_t* metadata);

#endif

#endif /* PHP_EXECUTE_HDR */
