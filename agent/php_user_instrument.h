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
#include "php_user_instrument_hashmap_key.h"

struct _nruserfn_t;

/*
 * This is an unused structure that is used to ensure that a bare return won't
 * compile in a user wrapper.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
typedef void (*nrspecialfn_t)(zend_execute_data*, ...);

#else
struct nrspecialfn_return_t {
  int zcaught;
};
typedef struct nrspecialfn_return_t (*nrspecialfn_t)(
    NR_SPECIALFNPTR_PROTO TSRMLS_DC);

#endif
typedef void (*nruserfn_declared_t)(TSRMLS_D);

/*
 * An equivalent data structure for user functions.
 *
 * Note that for this structure, the "char *" fields are just that,
 * and these references own the corresponding string,
 * and so the strings must be discarded in nr_php_user_wraprec_destroy;
 */
typedef struct _nruserfn_t {
  struct _nruserfn_t* next; /* singly linked list next pointer */

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  /* wraprec hashmap key */
  nr_php_wraprec_hashmap_key_t key;
#endif

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
   * As an alternative to the current implementation, this could be
   * converted to a linked list so that we can nest wrappers.
   */
  /*
   * This is the callback that legacy instrumentation uses and that the majority
   * of OAPI special instrumentation will use and it will be called at the END
   * of a function.
   */
  nrspecialfn_t special_instrumentation;
  /*
   * Only used by OAPI, PHP 8+.  Used to do any special instrumentation actions
   * before a function is executed. All callbacks can be set using
   * `nr_php_wrap_user_function_before_after`.
   */
  nrspecialfn_t special_instrumentation_before;

  nruserfn_declared_t declared_callback;

  int is_method;
  int is_disabled;
  int is_wrapped;
  int is_exception_handler;   /* 1 if the current exception handler; 0 otherwise
                               */
  int is_names_wt_simple;     /* True if this function "names" its enclosing WT;
                                 the first such function does the naming */
  bool is_transient;          /* Wraprecs that are transient are destroyed
                                 after each request. Wraprecs that are
                                 non-transient are kept until module shutdown.
                                 Currently, while all wraprecs are stored
                                 in a hashmap, non-transients are also
                                 stored in a linked list to repopulate
                                 the hash. This should be changed to no
                                 longer need the linked list. */
  int is_user_added;          /* True if user requested this instrumentation */
  int create_metric;          /* 1 if a metric should be made for calls of
                                 this function */

  char* drupal_module;
  nr_string_len_t drupal_module_len;
  char* drupal_hook;
  nr_string_len_t drupal_hook_len;
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  char* wordpress_plugin_theme;
#endif
} nruserfn_t;

extern nruserfn_t* nr_wrapped_user_functions; /* a singly linked list */

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO

/*
 * Purpose : Init user instrumentation. This must only be called on request
 * init! This creates wraprec lookup hashmap and registers wraprec destructor
 * callback which is called on request shutdown.
 *
 * Params  : None
 *
 * Returns : None
 */
extern void nr_php_init_user_instrumentation(void);
/*
 * Purpose : Get the wraprec associated with a zend_function.
 *
 * Params  : 1. The zend function to find a wraprec for
 *
 * Returns : The function wrapper that matches the zend_function or NULL if no
 * match was found.
 */
extern nruserfn_t* nr_php_get_wraprec(zend_function* zf);
#else
/*
 * Purpose : Get the wraprec associated with a user function op_array.
 *
 * Params  : 1. The zend function's oparray.
 *
 * Returns : The function wrapper. NULL if no function wrapper was registered
 *           or if the registered function wrapper is invalid.
 */
extern nruserfn_t* nr_php_op_array_get_wraprec(
    const zend_op_array* op_array TSRMLS_DC);

/*
 * Purpose : Set the wraprec associated with a user function op_array.
 *
 * Params  : 1. The zend function's oparray.
 *	     2. The function wrapper.
 */
extern void nr_php_op_array_set_wraprec(zend_op_array* op_array,
                                        nruserfn_t* func TSRMLS_DC);
#endif
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
                                                  size_t namestrlen);
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
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
extern int nr_zend_call_oapi_special_before(nruserfn_t* wraprec,
                                            nr_segment_t* segment,
                                            NR_EXECUTE_PROTO);
#endif
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

#endif /* PHP_USER_INSTRUMENT_HDR */
