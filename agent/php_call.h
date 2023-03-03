/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wrappers for PHP's call_user_function_ex() call.
 */
#ifndef PHP_CALL_HDR
#define PHP_CALL_HDR

/*
 * Purpose : Executes a PHP user function of arity zero. This is a wrapper
 *           around call_user_function_ex provided by PHP, which requires a
 *           bunch of additional parameters we (almost) always want to be NULL.
 *
 * Params  : 1. The optional instance object.
 *           2. The name of the user function to invoke.
 *           3. The number of parameters in "params".
 *           4. An array containing the parameter values.
 *
 * Returns : The returned value, or NULL if the function invocation failed.
 *           This must be destroyed with nr_php_zval_free().
 *
 * Warning : The handling of by reference parameters in PHP 7 has changed. You
 *           will need to provide a zval of type IS_REF, rather than relying on
 *           PHP implicitly changing the zval value for you. An example of this
 *           in the context of a pure out parameter can be found in
 *           nr_php_explain_mysqli_fetch_plan(), which was adapted from PHP's
 *           own stream wrapper code:
 *           https://github.com/php/php-src/blob/php-7.0.0RC1/main/streams/userspace.c#L371
 */
extern zval* nr_php_call_user_func(zval* object_ptr,
                                   const char* function_name,
                                   zend_uint param_count,
                                   zval* params[] TSRMLS_DC);

/*
 * Purpose : A friendlier wrapper for nr_php_call_user_func().
 *
 * Params  : 1.  The optional instance object.
 *           2.  The name of the user function to invoke.
 *           ... zval * parameters for the function.
 *
 * Note    : This can't be used if the number of parameters are calculated at
 *           runtime; use the underlying nr_php_call_user_func() function
 *           instead.
 */
#define nr_php_call(object_ptr, function_name, params...)             \
  ({                                                                  \
    zval* call_params[] = {params};                                   \
    size_t num_call_params = sizeof(call_params) / sizeof(zval*);     \
    nr_php_call_user_func(object_ptr, function_name, num_call_params, \
                          (num_call_params > 0 ? call_params : NULL)  \
                              TSRMLS_CC);                             \
  })

/*
 * Purpose : A variant of nr_php_call_user_func() that catches any exception
 *           thrown by the called user function and returns it via an out
 *           parameter, then clears the exception from PHP.
 *
 * Params  : 1. The optional instance object.
 *           2. The name of the user function to invoke.
 *           3. The number of parameters in "params".
 *           4. An array containing the parameter values.
 *           5. A pointer to a zval pointer that will receive an exception
 *              object if one is thrown. Ownership of this zval is transferred
 *              to the caller, and it must be freed with nr_php_zval_free()
 *              when no longer needed.
 *
 * Returns : The returned value, or NULL if the function invocation failed.
 *           This must be destroyed with nr_php_zval_free().
 */
extern zval* nr_php_call_user_func_catch(zval* object_ptr,
                                         const char* function_name,
                                         zend_uint param_count,
                                         zval* params[],
                                         zval** exception TSRMLS_DC);

extern zval* nr_php_call_callable_zval(zval* callable,
                                       zend_uint param_count,
                                       zval* params[] TSRMLS_DC);

#define nr_php_call_callable(callable, params...)                        \
  ({                                                                     \
    zval* call_params[] = {params};                                      \
    size_t num_call_params = sizeof(call_params) / sizeof(zval*);        \
    nr_php_call_callable_zval(callable, num_call_params,                 \
                              (num_call_params > 0 ? call_params : NULL) \
                                  TSRMLS_CC);                            \
  })

extern zval* nr_php_call_fcall_info_zval(zend_fcall_info fci,
                                         zend_fcall_info_cache fcc,
                                         zend_uint param_count,
                                         zval* params[] TSRMLS_DC);

#define nr_php_call_fcall_info(fci, fcc, params...)                        \
  ({                                                                       \
    zval* call_params[] = {params};                                        \
    size_t num_call_params = sizeof(call_params) / sizeof(zval*);          \
    nr_php_call_fcall_info_zval(fci, fcc, num_call_params,                 \
                                (num_call_params > 0 ? call_params : NULL) \
                                    TSRMLS_CC);                            \
  })

extern void nr_php_call_user_func_array_handler(
    nrphpcufafn_t handler,
    zend_function* func,
    zend_execute_data* prev_execute_data TSRMLS_DC);

extern zval* nr_php_call_offsetGet(zval* object_ptr, const char* key TSRMLS_DC);

#endif /* PHP_CALL_HDR */
