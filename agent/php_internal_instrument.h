/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains logic pertaining to the instrumentation of internal
 * functions (where the function is written in C).
 */
#ifndef PHP_INTERNAL_INSTRUMENT_HDR
#define PHP_INTERNAL_INSTRUMENT_HDR

/*
 * Note that the "char *" fields are "const char *" as these fields point
 * to compiler allocated strings from "...", and not from strings dynamically
 * allocated using, say, nr_strdup.  Thus, when freeing instances of this
 * structure, you do not need to free the outgoing references.
 */
typedef struct _nrinternalfn_t {
  struct _nrinternalfn_t* next; /* singly linked list next pointer */

  const char* full_name;

  const char* extra; /* extra naming information about the function */

  /*
   * The supportability metric name used to track calls to this function is
   * created at construction to avoid creating it at each call.  Although we
   * could have a count field here and make a metric at the end of the
   * transaction, that approach would not be thread safe.
   */
  char* supportability_metric;

  /*
   * Refer to the extensive documentation in php_internal_instrument.c
   * for information regarding the inner and outer wrappers, and the
   * outer_wrapper_global.
   */
  struct _nrinternalfn_t** outer_wrapper_global;
  nrphpfn_t outer_wrapper;
  void (*inner_wrapper)(INTERNAL_FUNCTION_PARAMETERS,
                        struct _nrinternalfn_t* fn); /* Can't use
                                                        nr_inner_wrapper_fn_t
                                                        due to circular
                                                        reference */
  nrphpfn_t oldhandler;
  int is_disabled;
  int is_wrapped;
} nrinternalfn_t;

/*
 * Remember the original handlers for each of the instrumented functions. These
 * are the code blocks that we execute inside the instrumentation wrappers.
 * This is the head of a dynamically allocated singly linked list of wrappers.
 */
extern nrinternalfn_t* nr_wrapped_internal_functions;

/*
 * Purpose : Populate nr_wrapped_internal_functions.  This runtime generation
 *           is more flexible and less brittle than a static array approach.
 */
extern void nr_php_generate_internal_wrap_records(void);

extern void nr_php_add_internal_instrumentation(TSRMLS_D);

/*
 * Purpose : Wrap an existing function with an instrumentation function. Our
 *           technique is to find both functions in the function_table and
 *           then to replace just the handler function pointer of the wrapped
 *           function: everything else in the function_table stays the same.
 *
 * Params  : 1. The function to be wrapped.
 *
 */
extern void nr_php_wrap_internal_function(nrinternalfn_t* wraprec TSRMLS_DC);

extern int nr_zend_call_old_handler(nrphpfn_t oldhandler,
                                    INTERNAL_FUNCTION_PARAMETERS);

/*
 * Purpose : Destroy internal function instrumentation records, freeing
 *           associated memory.
 *
 * Notes   : WARNING This function should only be called when PHP is shutting
 *           down and no more PHP execution will happen in this process.  This
 *           is important, since internal functions hold pointers our
 *           instrumentation functions, and without the wrap records they cannot
 *           dispatch correctly.
 */
extern void nr_php_destroy_internal_wrap_records(void);

extern void nr_php_add_call_user_func_array_pre_callback(
    nrphpcufafn_t callback TSRMLS_DC);

#endif /* PHP_INTERNAL_INSTRUMENT_HDR */
