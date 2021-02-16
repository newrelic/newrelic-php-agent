/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "util_logging.h"

#include "Zend/zend_exceptions.h"

zval* nr_php_call_user_func(zval* object_ptr,
                            const char* function_name,
                            zend_uint param_count,
                            zval* params[] TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  int zend_result = FAILURE;
  zval* fname = NULL;
  HashTable* symbol_table = NULL;
  zval* param_values = NULL;
  zval* retval = NULL;
#ifndef PHP8
  int no_separation = 0;
#endif /* PHP8 */

  /*
   * Prevent an unused variable warning. PHP 7.1 changed call_user_function_ex
   * to a macro and removed the symbol_table argument from the underlying
   * function.
   */
  (void)symbol_table;

  if ((NULL == function_name) || (function_name[0] == '\0')) {
    return NULL;
  }

  if ((NULL != params) && (param_count > 0)) {
    zend_uint i;

    param_values = (zval*)nr_calloc(param_count, sizeof(zval));
    for (i = 0; i < param_count; i++) {
      param_values[i] = *params[i];
    }
  }

  retval = nr_php_zval_alloc();

  fname = nr_php_zval_alloc();
  nr_php_zval_str(fname, function_name);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */

  /*
   * With PHP8, `call_user_function_ex` was removed and `call_user_function`
   * became the recommended function.
   */
  zend_result = call_user_function(EG(function_table), object_ptr, fname,
                                   retval, param_count, param_values);

#else
  zend_result = call_user_function_ex(EG(function_table), object_ptr, fname,
                                      retval, param_count, param_values,
                                      no_separation, symbol_table TSRMLS_CC);
#endif /* PHP8+ */
  nr_php_zval_free(&fname);

  nr_free(param_values);

  if (SUCCESS == zend_result) {
    return retval;
  }

  nr_php_zval_free(&retval);
  return NULL;
#else /* PHP < 7 */
  int zend_result;
  zval* fname = NULL;
  int no_separation = 0;
  HashTable* symbol_table = NULL;
  zval*** param_ptrs = NULL;
  zval* retval = NULL;

  if ((NULL == function_name) || (function_name[0] == '\0')) {
    return NULL;
  }

  if ((NULL != params) && (param_count > 0)) {
    zend_uint i;

    param_ptrs = (zval***)nr_calloc(param_count, sizeof(zval**));
    for (i = 0; i < param_count; i++) {
      param_ptrs[i] = &params[i];
    }
  }

  fname = nr_php_zval_alloc();
  nr_php_zval_str(fname, function_name);
  zend_result = call_user_function_ex(EG(function_table), &object_ptr, fname,
                                      &retval, param_count, param_ptrs,
                                      no_separation, symbol_table TSRMLS_CC);
  nr_php_zval_free(&fname);

  nr_free(param_ptrs);

  if (SUCCESS == zend_result) {
    return retval;
  }

  nr_php_zval_free(&retval);
  return NULL;
#endif
}

zval* nr_php_call_user_func_catch(zval* object_ptr,
                                  const char* function_name,
                                  zend_uint param_count,
                                  zval* params[],
                                  zval** exception TSRMLS_DC) {
  zval* retval = NULL;

  /*
   * Return if exception is NULL (which really shouldn't happen!).
   */
  if (NULL == exception) {
    return nr_php_call_user_func(object_ptr, function_name, param_count,
                                 params TSRMLS_CC);
  }

  /*
   * The approach for both PHP 5, 7, and 8 is conceptually the same: persist the
   * current exception pointer in the executor globals, then compare it after
   * the call has been made. If the pointer changes, then an exception was
   * thrown. The key difference is that PHP 5 uses a zval, whereas PHP 7+ uses
   * a zend_object.
   */

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  {
    zend_object* exception_obj = EG(exception);

    retval
        = nr_php_call_user_func(object_ptr, function_name, param_count, params);

    if ((NULL != EG(exception)) && (EG(exception) != exception_obj)) {
      zval* exception_zv = nr_php_zval_alloc();

      /*
       * Wrap EG(exception) in a zval for API consistency with PHP 5, ensuring
       * that we increment the refcount so that the caller's subsequent
       * nr_php_zval_free() call does the right thing.
       */

      ZVAL_OBJ(exception_zv, EG(exception));
      Z_ADDREF_P(exception_zv);

      *exception = exception_zv;
      zend_clear_exception();
    }
  }
#else
  {
    zval* exception_zv = EG(exception);

    retval = nr_php_call_user_func(object_ptr, function_name, param_count,
                                   params TSRMLS_CC);

    if ((NULL != EG(exception)) && (EG(exception) != exception_zv)) {
      Z_ADDREF_P(EG(exception));
      *exception = EG(exception);
      zend_clear_exception(TSRMLS_C);
    }
  }
#endif /* PHP7+ */

  return retval;
}

zval* nr_php_call_callable_zval(zval* callable,
                                zend_uint param_count,
                                zval* params[] TSRMLS_DC) {
  char* error = NULL;
  zend_fcall_info_cache fcc;
  zend_fcall_info fci;

  if (!nr_php_is_zval_valid_callable(callable TSRMLS_CC)) {
    return NULL;
  }

  if (SUCCESS
      != zend_fcall_info_init(callable, 0, &fci, &fcc, NULL, NULL TSRMLS_CC)) {
    nrl_verbosedebug(NRL_AGENT, "%s: error in zend_fcall_info_init: %s",
                     __func__, NRSAFESTR(error));
    efree(error);
    return NULL;
  }

  return nr_php_call_fcall_info_zval(fci, fcc, param_count, params TSRMLS_CC);
}

zval* nr_php_call_fcall_info_zval(zend_fcall_info fci,
                                  zend_fcall_info_cache fcc,
                                  zend_uint param_count,
                                  zval* params[] TSRMLS_DC) {
#ifdef PHP7
  zend_uint i;

  if ((NULL != params) && (param_count > 0)) {
    fci.param_count = (uint32_t)param_count;
    fci.params = (zval*)nr_calloc(param_count, sizeof(zval));
    for (i = 0; i < param_count; i++) {
      fci.params[i] = *params[i];
    }
  }

  fci.retval = nr_php_zval_alloc();

  if (SUCCESS != zend_call_function(&fci, &fcc)) {
    nr_php_zval_free(&fci.retval);
  }

  nr_free(fci.params);
  return fci.retval;
#else
  zend_uint i;
  zval* retval = NULL;

  if ((NULL != params) && (param_count > 0)) {
    fci.param_count = (uint32_t)param_count;
    fci.params = (zval***)nr_calloc(param_count, sizeof(zval**));
    for (i = 0; i < param_count; i++) {
      fci.params[i] = &params[i];
    }
  }

  /*
   * We don't need to allocate retval; the Zend Engine will do that for us when
   * the function returns a value.
   */
  fci.retval_ptr_ptr = &retval;
  if (SUCCESS != zend_call_function(&fci, &fcc TSRMLS_CC)) {
    nr_php_zval_free(&retval);
  }

  nr_free(fci.params);
  return retval;
#endif /* PHP7 */
}

void nr_php_call_user_func_array_handler(nrphpcufafn_t handler,
                                         zend_function* func,
                                         zend_execute_data* prev_execute_data
                                             TSRMLS_DC) {
  const zend_function* caller = NULL;

  if (prev_execute_data) {
#ifdef PHP7
    caller = prev_execute_data->func;
#else
    caller = prev_execute_data->function_state.function;
#endif /* PHP7 */
  } else {
#if ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
    caller = nr_php_get_caller(EG(current_execute_data), 1 TSRMLS_CC);
#else
    caller = nr_php_get_caller(NULL, 1 TSRMLS_CC);
#endif /* PHP >= 5.5 */
  }

  (handler)(func, caller TSRMLS_CC);
}

zval* nr_php_call_offsetGet(zval* object_ptr, const char* key TSRMLS_DC) {
  zval* key_zv = NULL;
  zval* retval = NULL;

  key_zv = nr_php_zval_alloc();
  nr_php_zval_str(key_zv, key);

  retval = nr_php_call_user_func(object_ptr, "offsetGet", 1, &key_zv TSRMLS_CC);

  nr_php_zval_free(&key_zv);
  return retval;
}
