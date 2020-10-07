/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_WRAPPER_HDR
#define PHP_WRAPPER_HDR

#include "php_user_instrument.h"
#include "util_logging.h"

/*
 * Wrapper writing example:
 *
 * Using the functions below, you can register a wrapper for either a named
 * user function or directly on a zend_function pointer. This wrapper needs to
 * have the following rough structure:
 *
 * NR_PHP_WRAPPER (my_awesome_wrapper)
 * {
 *   zval *arg;
 *   zval **retval_ptr;
 *   zval *this_var;
 *
 *   // for wrappers that require a particular framework to be detected:
 *   NR_PHP_WRAPPER_REQUIRE_FRAMEWORK (NR_FW_FOOBAR);
 *
 *   // or for wrappers that require a particular framework version:
 *   NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION (NR_FW_FOOBAR, 4);
 *
 *   // if required, get the parameters, return value pointer, and scope; eg:
 *   arg = nr_php_arg_get (1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
 *   retval_ptr = nr_php_get_return_value_ptr (TSRMLS_C);
 *   this_var = nr_php_scope_get (NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
 *
 *   // do awesome stuff before the function
 *
 *   // call the original function
 *   NR_PHP_WRAPPER_CALL;
 *
 *   // do awesome stuff after the function
 *
 *   // make sure you release any arguments and/or scope variables; eg:
 *   nr_php_arg_release (&arg);
 *   nr_php_scope_release (&this_var);
 * } NR_PHP_WRAPPER_END
 *
 * The critical thing to remember is that you can't access anything from the
 * given execute_data variable (which is encapsulated within
 * NR_EXECUTE_ORIG_ARGS) once NR_PHP_WRAPPER_CALL has been called, as the
 * execute_data and the fields within it may have been freed by the Zend
 * Engine.
 *
 * It is also important to release any arguments or scope variables that you
 * have accessed, lest you create memory leaks. Return values do not need to be
 * released, as they are owned by the Zend Engine and cannot be destroyed
 * before the wrapper function exits.
 *
 * Note that if you don't call the original function at the end of the wrapper,
 * it will be called for you. Please don't do that. Also don't set was_executed
 * to pretend that you called the function: PHP 5.5 onwards will crash if you
 * don't actually call the original function.
 *
 * Checklist for writing a user function wrapper:
 * 1. Any call to nr_php_arg_get, nr_php_scope_get, or
 *    nr_php_get_return_value_ptr follows any NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_*
 *    calls and precedes any NR_PHP_WRAPPER_CALL.
 *
 * 2. Values from nr_php_arg_get and nr_php_scope_get are released by the end of
 *    the function.
 *
 * 3. Wrapped functions (almost) always need to call NR_PHP_WRAPPER_CALL
 *    explicitly and it cannot be called more than once per code path.
 *
 * Special cases:
 * 1. By default, NR_PHP_WRAPPER will declare functions as static. If you need
 *    them to be exported, you can use NR_PHP_WRAPPER_START instead (which
 *    omits the static keyword) to open the wrapper function.
 *
 * 2. The NR_PHP_WRAPPER_PROTOTYPE macro can be used to define a prototype for
 *    an exported wrapper function in a header file.
 *
 * 3. Delegation: you can delegate from any wrapper to another wrapper with
 *    NR_PHP_WRAPPER_DELEGATE (foo), provided the original function hasn't
 *    already been called.
 */

extern nruserfn_t* nr_php_wrap_user_function(const char* name,
                                             size_t namelen,
                                             nrspecialfn_t callback TSRMLS_DC);

extern nruserfn_t* nr_php_wrap_user_function_extra(const char* name,
                                                   size_t namelen,
                                                   nrspecialfn_t callback,
                                                   const char* extra TSRMLS_DC);

extern nruserfn_t* nr_php_wrap_callable(zend_function* callable,
                                        nrspecialfn_t callback TSRMLS_DC);

/*
 * Purpose : Retrieve an argument from the current execute data.
 *
 * Params  : 1. The 1-indexed index of the argument.
 *           2. The PHP version specific execute data.
 *
 * Returns : A duplicate of the argument (created with ZVAL_DUP), or NULL if an
 *           error occurs. The argument must be released with
 *           nr_php_arg_release() when no longer required.
 *
 * Warning : This function MUST only be called in a user function wrapper, and
 *           MUST be called before the user function is executed.
 */
extern zval* nr_php_arg_get(ssize_t index, NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Release an argument retrieved with nr_php_arg_get().
 *
 * Params  : 1. A pointer to the argument.
 */
extern void nr_php_arg_release(zval** ppzv);

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
/*
 * Purpose : Add an argument to the current execute data.
 *
 *           In case the caller doesn't specify values for default parameters,
 *           this function can be used to add values for those parameters.
 *
 * Params  : 1. The PHP version specific execute data.
 *           2. The argument to be added. The zval will be duplicated, so
 *              ownership remains with the caller.
 *
 * Returns : true if the argument could be added.
 *
 * Warning : This function MUST only be called in a user function wrapper, and
 *           MUST be called before the user function is executed.
 *
 *           This function DOES NOT consider and alter extra arguments
 *           (arguments handled by the splat operator or by func_get_args).
 */
bool nr_php_arg_add(NR_EXECUTE_PROTO, zval* newarg);
#endif

/*
 * Purpose : Retrieve the current object scope ($this, in PHP), ensuring that
 *           the refcount is incremented so that the scope isn't destroyed
 *           before it is released.
 *
 * Params  : 1. The PHP version specific execute data.
 *
 * Returns : The scope, or NULL if an error occurs or the current function is
 *           not a method call. The scope must be released with
 *           nr_php_scope_release() when no longer required.
 *
 * Warning : This function MUST only be called in a user function wrapper, and
 *           MUST be called before the user function is executed.
 */
extern zval* nr_php_scope_get(NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Release a scope retrieved with nr_php_scope_get().
 *
 * Params  : 1. A pointer to the scope.
 */
extern void nr_php_scope_release(zval** ppzv);

/*
 * Purpose : Retrieve a pointer to the return value for the current function.
 *
 * Params  : 1. The PHP version specific execute data.
 *
 * Returns : A pointer to the return value. This value must not be accessed
 *           before the function has been executed, as it may be uninitialised
 *           until after execution.
 *
 * Warning : This function MUST only be called in a user function wrapper, and
 *           MUST be called before the user function is executed.
 */
extern zval** nr_php_get_return_value_ptr(TSRMLS_D);

#define NR_PHP_WRAPPER_PROTOTYPE(name) \
  struct nrspecialfn_return_t name(NR_SPECIALFNPTR_PROTO TSRMLS_DC)

#define NR_PHP_WRAPPER_START(name)                          \
  NR_PHP_WRAPPER_PROTOTYPE(name) {                          \
    int was_executed = 0;                                   \
    int zcaught = 0;                                        \
    const nrtxn_t* txn = NRPRG(txn);                        \
    const nrtime_t txn_start_time = nr_txn_start_time(txn); \
                                                            \
    (void)auto_segment;  // auto_segment isn't generally used in wrapper
                         // functions

#define NR_PHP_WRAPPER(name) static NR_PHP_WRAPPER_START(name)

#define NR_PHP_WRAPPER_END                           \
  callback_end:                                      \
  __attribute__((unused));                           \
  if (!was_executed) {                               \
    NR_PHP_WRAPPER_CALL                              \
  }                                                  \
                                                     \
  if (zcaught) {                                     \
    zend_bailout();                                  \
  }                                                  \
  {                                                  \
    struct nrspecialfn_return_t _retval = {zcaught}; \
    return _retval;                                  \
  }                                                  \
  }

#define NR_PHP_WRAPPER_CALL                                                 \
  if (!was_executed) {                                                      \
    zcaught = nr_zend_call_orig_execute(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);    \
    was_executed = 1;                                                       \
                                                                            \
    if (nrunlikely(NRPRG(txn) != txn                                        \
                   || nr_txn_start_time(NRPRG(txn)) != txn_start_time)) {   \
      nrl_verbosedebug(NRL_TXN,                                             \
                       "%s: transaction restarted during wrapped function " \
                       "call; clearing the segment pointer",                \
                       __func__);                                           \
      auto_segment = NULL;                                                  \
    }                                                                       \
  }

#define NR_PHP_WRAPPER_LEAVE goto callback_end;

#define NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(fw)                               \
  do {                                                                     \
    nrframework_t _required_fw = (fw);                                     \
                                                                           \
    if (_required_fw != NRPRG(current_framework)) {                        \
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: expected framework %d; got %d", \
                       __func__, _required_fw, NRPRG(current_framework));  \
      NR_PHP_WRAPPER_LEAVE;                                                \
    }                                                                      \
  } while (0)

#define NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(fw, ver)                   \
  do {                                                                      \
    nrframework_t _required_fw = (fw);                                      \
    int _required_version = (ver);                                          \
                                                                            \
    if ((_required_fw != NRPRG(current_framework))                          \
        || (_required_version != NRPRG(framework_version))) {               \
      nrl_verbosedebug(NRL_FRAMEWORK,                                       \
                       "%s: expected framework %d ver %d; got %d ver %d",   \
                       __func__, _required_fw, _required_version,           \
                       NRPRG(current_framework), NRPRG(framework_version)); \
      NR_PHP_WRAPPER_LEAVE;                                                 \
    }                                                                       \
  } while (0)

#define NR_PHP_WRAPPER_DELEGATE(name)                                \
  if (!was_executed) {                                               \
    zcaught = ((name)(NR_SPECIALFNPTR_ORIG_ARGS TSRMLS_CC)).zcaught; \
    was_executed = 1;                                                \
  }

#endif /* PHP_WRAPPER_HDR */
