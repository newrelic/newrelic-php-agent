/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains PHP Execution hooks.
 */
#ifndef PHP_EXECUTE_HDR
#define PHP_EXECUTE_HDR

/*
 * An op_array is for a file rather than a function if it has a file name
 * and no function name.
 */
#define OP_ARRAY_IS_A_FILE(OP) \
  ((0 == nr_php_op_array_function_name(OP)) && nr_php_op_array_file_name(OP))
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

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
/*
 * Purpose : Call the necessary functions needed to instrument a function by
 *           starting a transaction for a function that has just started.  This
 *           function is registered via the Observer API and will be called by
 *           the zend engine every time a function begins.  The zend engine
 *           directly provides the zend_execute_data which has all details we
 *           need to know about the function.
 *
 *
 * Params  : 1. zend_execute_data: everything we need to know about the
 * function.
 *
 * Returns : Void.
 */
void nr_php_execute_observer_fcall_begin(zend_execute_data* execute_data);
/*
 * Purpose : Call the necessary functions needed to instrument a function when
 *           ending a transaction for a function that has just ended.  This
 *           function is registered via the Observer API and will be called by
 *           the zend engine every time a function ends.  The zend engine
 *           directly provides the zend_execute_data and teh return_value
 *           pointer, both of which have all details that the agent needs to
 *           know about the function.
 *
 *
 * Params  : 1. zend_execute_data: everything to know about the function.
 *           2. return_value: function return value information
 *
 * Returns : Void.
 */
void nr_php_execute_observer_fcall_end(zend_execute_data* execute_data,
                                       zval* return_value);
#endif

#endif /* PHP_EXECUTE_HDR */
