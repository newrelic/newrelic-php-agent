/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "util_logging.h"

nruserfn_t* nr_php_wrap_user_function(const char* name,
                                      size_t namelen,
                                      nrspecialfn_t callback TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_named(name, namelen TSRMLS_CC);

  if (wraprec && callback) {
    if ((NULL != wraprec->special_instrumentation)
        && (callback != wraprec->special_instrumentation)) {
      nrl_verbosedebug(
          NRL_INSTRUMENT,
          "%s: attempting to set special_instrumentation for %.*s, but "
          "it is already set",
          __func__, NRSAFELEN(namelen), NRBLANKSTR(name));
    } else {
      wraprec->special_instrumentation = callback;
    }
  }

  return wraprec;
}

nruserfn_t* nr_php_wrap_user_function_extra(const char* name,
                                            size_t namelen,
                                            nrspecialfn_t callback,
                                            const char* extra TSRMLS_DC) {
  nruserfn_t* wraprec
      = nr_php_wrap_user_function(name, namelen, callback TSRMLS_CC);
  wraprec->extra = extra;

  return wraprec;
}

nruserfn_t* nr_php_wrap_callable(zend_function* callable,
                                 nrspecialfn_t callback TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_callable(callable TSRMLS_CC);

  if (wraprec && callback) {
    if ((NULL != wraprec->special_instrumentation)
        && (callback != wraprec->special_instrumentation)) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "%s: attempting to set special_instrumentation, but "
                       "it is already set",
                       __func__);
    } else {
      wraprec->special_instrumentation = callback;
    }
  }

  return wraprec;
}

inline static void release_zval(zval** ppzv) {
#ifdef PHP7
  nr_php_zval_free(ppzv);
#else
  if (NULL == ppzv) {
    return;
  }
  if (NULL == *ppzv) {
    return;
  }

  zval_ptr_dtor(ppzv);
  *ppzv = NULL;
#endif /* PHP7 */
}

zval* nr_php_arg_get(ssize_t index, NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* arg;

#ifdef PHP7
  {
    zval* orig;

    arg = NULL;
    orig = nr_php_get_user_func_arg((zend_uint)index, NR_EXECUTE_ORIG_ARGS);

    if (orig) {
      arg = nr_php_zval_alloc();
      ZVAL_DUP(arg, orig);
    }
  }
#else
  arg = nr_php_get_user_func_arg((zend_uint)index,
                                 NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (arg) {
    Z_ADDREF_P(arg);
  }
#endif /* PHP7 */

  return arg;
}

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
/*
 * This function can be used to add arguments to a PHP function call in
 * the wrapper function. This is done by manipulating the current stack
 * frame (execute context) which is passed into the wapper.
 *
 * This is the layout of a stack frame, as described in
 * https://nikic.github.io/2017/04/14/PHP-7-Virtual-machine.html#stack-frame-layout:
 *
 * +----------------------------------------+
 * | zend_execute_data                      |
 * +----------------------------------------+
 * | VAR[0]                =         ARG[1] | arguments
 * | ...                                    |
 * | VAR[num_args-1]       =         ARG[N] |
 * | VAR[num_args]         =   CV[num_args] | remaining CVs
 * | ...                                    |
 * | VAR[last_var-1]       = CV[last_var-1] |
 * | VAR[last_var]         =         TMP[0] | TMP/VARs
 * | ...                                    |
 * | VAR[last_var+T-1]     =         TMP[T] |
 * | ARG[N+1] (extra_args)                  | extra arguments
 * | ...                                    |
 * +----------------------------------------+
 *
 * Each PHP stack frame is allocated on the VM stack and, amongst other things,
 * contains:
 *
 *  - zval slots for each argument of the function definition (VAR). These
 *    slots can be addressed via the ZEND_CALL_ARG macro and an index
 *    (starting with 1).
 *  - A counter that holds the number of arguments given to the function call.
 *    This counter can be obtained via the ZEND_CALL_NUM_ARGS macro. All
 *    zval argument slots with an index less than or equal to the value of
 *    this counter are initialized when the wrapper is called. zval
 *    argument with an index greater than the value of this counter are
 *    uninitialized when the wrapper is called.
 *  - A pointer to the zend_function that is called. This zend_function
 *    holds a counter that specifies the number of arguments that were
 *    defined to that function. This counter includes default arguments,
 *    but does not include extra arguments.
 *
 * This function does the following:
 *
 *  1. It checks if there is an uninitialized zval argument slot. It
 *     does so by comparing the counter for defined arguments (in
 *     the zend_function) with the counter of arguments given in the
 *     call (in the stack frame, accessed via ZEND_CALL_NUM_ARGS).
 *  2. It obtains the uninitialized zval argument slot.
 *  3. It copies the given zval into the slot.
 *
 * If there is no uninitialized zval argument slot, this function does
 * nothing and returns false.
 *
 * This function does not alter extra arguments (arguments defined with
 * the splat operator or returned by func_get_args). This could by done
 * by manipulating the ARG[N+1] slot pictured in the stack frame layout
 * above. However, there is currently no requirement for doing that.
 */
bool nr_php_arg_add(NR_EXECUTE_PROTO, zval* newarg) {
  zval* orig;
  size_t num_args;
  size_t max_args;
  zend_execute_data* ex = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS);

  if (NULL == newarg || NULL == ex) {
    return false;
  }

  /*
   * Check not to add more arguments than the function has defined.
   */
  num_args = ZEND_CALL_NUM_ARGS(ex) + 1;
  max_args = ex->func->common.num_args;

  if (num_args > max_args) {
    return false;
  }

  /*
   * Increase the count of given arguments.
   */
  ZEND_CALL_NUM_ARGS(ex) = num_args;

  /*
   * Copy the new argument into the zval argument slot in the function.
   */
  orig = ZEND_CALL_ARG(ex, num_args);
  ZVAL_DUP(orig, newarg);

  return true;
}
#endif

void nr_php_arg_release(zval** ppzv) {
  release_zval(ppzv);
}

zval* nr_php_scope_get(NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* this_obj;
  zval* this_copy;

  this_obj = NR_PHP_USER_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return NULL;
  }

#ifdef PHP7
  this_copy = nr_php_zval_alloc();
  ZVAL_DUP(this_copy, this_obj);
#else
  NR_UNUSED_SPECIALFN;

  this_copy = this_obj;
  Z_ADDREF_P(this_copy);
#endif

  return this_copy;
}

void nr_php_scope_release(zval** ppzv) {
  release_zval(ppzv);
}

zval** nr_php_get_return_value_ptr(TSRMLS_D) {
#ifdef PHP7
  if (NULL == EG(current_execute_data)) {
    return NULL;
  }

  return &EG(current_execute_data)->return_value;
#else
  return EG(return_value_ptr_ptr);
#endif /* PHP7 */
}
