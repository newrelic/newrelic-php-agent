/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains logic pertaining to the instrumentation of user functions
 * (where the function is written in PHP).
 */
#ifndef PHP_USER_INSTRUMENT_HDR
#define PHP_USER_INSTRUMENT_HDR

#include "nr_segment.h"


struct _nruserfn_t;

/*
 * This is an unused structure that is used to ensure that a bare return won't
 * compile in a user wrapper.
 */
struct nrspecialfn_return_t {
  int zcaught;
};
typedef struct nrspecialfn_return_t (*nrspecialfn_t)(
    NR_SPECIALFNPTR_PROTO TSRMLS_DC);

typedef void (*nruserfn_declared_t)(TSRMLS_D);

typedef struct {
  bool is_set;
  uint32_t len;
  union {
    char *cp; /* cp = char pointer */
    const char *ccp; /* ccp = const char pointer */
  } value;
} zf_metadata_t;
typedef struct _nruserfn_metadata {
  uint32_t lineno;
  zf_metadata_t filename;
  zf_metadata_t scope;
  zf_metadata_t function_name;
} nruserfn_metadata;

/*
 * An equivalent data structure for user functions.
 *
 * Note that for this structure, the "char *" fields are just that,
 * and these references own the corresponding string,
 * and so the strings must be discarded in nr_php_user_wraprec_destroy;
 */
typedef struct _nruserfn_t {
  struct _nruserfn_t* next; /* singly linked list next pointer */

  nruserfn_metadata id;

  const char* extra; /* extra naming information about the function */

  char* classname;
  char* classnameLC;
  int classnamelen;

  /*
   * The supportability metric name used to track calls to this function is
   * created at construction to avoid creating it at each call.  Although we
   * could have a count field here and make a metric at the end of the
   * transaction, that approach would not be thread safe.
   */
  char* supportability_metric;

  char* funcname;
  int funcnamelen;
  char* funcnameLC;

  /*
   * Internally, there are cases where the zend_function reports it is one
   * class; however, the zend_function is also contained in another class_entry
   * table.
   * For an example, see tests/integration/laravel
   * A lookup for the class = Illuminate\Console\Application returns the class
   * entry named classname = Illuminate\Console\Application
   * So far so good!
   * Lookup Illuminate\Console\Application method doRun and a zend_function is
   * returned.  Ask that zend_func what its classname is and it says:
   * Symfony\Component\Console\Application.
   * Okay.
   * Track both pieces of info for any wraprecs.
   */
  char* reportedclass;
  /*
   * These are helpful to compare the wraprec more quickly and to differentiate
   * between closures.
   */
  char* filename;
  uint32_t lineno;
  /*
   * As an alternative to the current implementation, this could be
   * converted to a linked list so that we can nest wrappers.
   */
  nrspecialfn_t special_instrumentation;

  nruserfn_declared_t declared_callback;

  int is_method;
  int is_disabled;
  int is_wrapped;
  int is_exception_handler; /* 1 if the current exception handler; 0 otherwise
                             */
  int is_names_wt_simple; /* True if this function "names" its enclosing WT; the
                             first such function does the naming */
  int is_transient;       /* Non-zero if specific to the request */
  int is_user_added;      /* True if user requested this instrumentation */
  int create_metric; /* 1 if a metric should be made for calls of this function
                      */

  char* drupal_module;
  nr_string_len_t drupal_module_len;
  char* drupal_hook;
  nr_string_len_t drupal_hook_len;
} nruserfn_t;

extern nruserfn_t* nr_wrapped_user_functions; /* a singly linked list */

// clang-format off
/* Wrappers around different implementations of enabling and retrieving instrumentation */
extern void nr_php_user_instrument_set(zend_function* func, nruserfn_t* wraprec);
extern nruserfn_t* nr_php_user_instrument_get(zend_function* func);
// clang-format on

/*
 * Purpose : Name a transaction
 *
 * Params  : 1. The name string of the transaction.
 *	     2. Length of the namestring.
 */
extern void nr_php_add_transaction_naming_function(const char* namestr,
                                                   int namestrlen TSRMLS_DC);

/*
 * Purpose : Add a custom tracer.
 *
 * Params  : 1. The name string of the custom tracer.
 *	     2. Length of the name string.
 */
extern void nr_php_add_custom_tracer(const char* namestr,
                                     int namestrlen TSRMLS_DC);

extern nruserfn_t* nr_php_add_custom_tracer_callable(
    zend_function* func TSRMLS_DC);
extern nruserfn_t* nr_php_add_custom_tracer_named(const char* namestr,
                                                  size_t namestrlen TSRMLS_DC);
extern void nr_php_reset_user_instrumentation(void);
extern void nr_php_remove_transient_user_instrumentation(void);
extern void nr_php_add_user_instrumentation(TSRMLS_D);

/*
 * Purpose : Name a zend framework transaction
 *
 * Params  : 1. The name string.
 *	     2. Length of the namestring.
 */
extern void nr_php_add_zend_framework_transaction_naming_function(
    char* namestr,
    int namestrlen TSRMLS_DC);

/*
 * Purpose : Wrap a user function and flag it as the active exception handler.
 *
 * Params  : 1. The user function to wrap and flag.
 */
extern void nr_php_add_exception_function(zend_function* func TSRMLS_DC);

/*
 * Purpose : Remove the flag from a user function wrapper denoting that it is
 *           the active exception handler.
 *
 * Params  : 1. The user function to unflag.
 */
extern void nr_php_remove_exception_function(zend_function* func TSRMLS_DC);

extern int nr_zend_call_orig_execute(NR_EXECUTE_PROTO TSRMLS_DC);
extern int nr_zend_call_orig_execute_special(nruserfn_t* wraprec,
                                             nr_segment_t* segment,
                                             NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Destroy all user instrumentation records, freeing
 *           associated memory.
 *
 * Notes   : WARNING This function should only be called when PHP is shutting
 *           down and no more PHP execution will happen in this process.  This
 *           is important, since op_arrays hold pointers to user function
 *           wrap records.
 */
extern void nr_php_destroy_user_wrap_records(void);

/*
 * Purpose : Add a callback that is fired when a function is declared.
 *
 * Params  : 1. The function name.
 *           2. The length of the function name.
 *           3. The callback to fire when the function is declared.
 */
extern void nr_php_user_function_add_declared_callback(
    const char* namestr,
    int namestrlen,
    nruserfn_declared_t callback TSRMLS_DC);

static inline bool chk_reported_class(const zend_function* func,
                                      nruserfn_t* wraprec) {
  if ((NULL == func) || (NULL == func->common.scope)) {
    return false;
  }

  if ((NULL == func->common.scope->name) || (NULL != wraprec->reportedclass)) {
    return false;
  }
  return true;
}

#endif /* PHP_USER_INSTRUMENT_HDR */
