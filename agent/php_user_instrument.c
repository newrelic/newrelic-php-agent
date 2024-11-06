/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument.h"
#include "php_user_instrument_hashmap.h"
#include "php_wrapper.h"
#include "lib_guzzle_common.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * The mechanism of zend_try .. zend_catch .. zend_end_try
 * is isolated into a handful of functions below.
 *
 * These are standalone functions so that the setjmp/longjmp
 * entailed in the implementation of them has a well defined stack frame,
 * without any variable sized objects in that stack frame, thus
 * giving longjmp a simple well defined place to come back to.
 * Having these standalone functions eliminates gcc compiler warning messages
 * -Wclobbered.
 *
 * These functions call through to the wrapped handler in various
 * ways. These C function does not create another stack frame so the
 * instrumentation is invisible to all the php introspection functions,
 * e.g., stack dumps, etc.
 *
 * The zend internal exception throwing mechanism (which triggers the
 * zend_try, zend_catch and zend_end_try code blocks) is used when:
 *   (a) there's an internal error in the zend engine, including:
 *     (1) bad code byte;
 *     (2) corrupted APC cache;
 *   (b) the PHP program calls exit.
 *   (c) An internal call to zend_error_cb, as for example empirically due to
 * one of: E_ERROR, E_PARSE, E_CORE_ERROR, E_CORE_WARNING, E_COMPILE_ERROR,
 * E_COMPILE_WARNING Cases (b) and (c) are interesting, as it is not
 * really an error condition, but merely a fast path out of the interpreter.
 *
 * Note that zend exceptions are NOT thrown when PHP throws exceptions;
 * PHP exceptions are handled at a higher layer.
 *
 * Note that if the wrapped function throws a zend exception,
 * the New Relic post dispatch handler is not called.
 *
 * Many functions here call zend_bailout to continue handling fatal PHP errors,
 * Since zend_bailout calls longjmp it never returns.
 *
 */
int nr_zend_call_orig_execute(NR_EXECUTE_PROTO TSRMLS_DC) {
  volatile int zcaught = 0;
  NR_UNUSED_FUNC_RETURN_VALUE;
  zend_try {
    NR_PHP_PROCESS_GLOBALS(orig_execute)
    (NR_EXECUTE_ORIG_ARGS_OVERWRITE TSRMLS_CC);
  }
  zend_catch { zcaught = 1; }
  zend_end_try();
  return zcaught;
}
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO /* PHP8+ */
int nr_zend_call_oapi_special_before(nruserfn_t* wraprec,
                                     nr_segment_t* segment,
                                     NR_EXECUTE_PROTO) {
  volatile int zcaught = 0;
  (void)segment;

  if (wraprec && wraprec->special_instrumentation_before) {
    zend_try {
      wraprec->special_instrumentation_before(NR_EXECUTE_ORIG_ARGS);
    }
    zend_catch { zcaught = 1; }
    zend_end_try();
  }

  return zcaught;
}
#else
int nr_zend_call_oapi_special_before(nruserfn_t* wraprec,
                                     nr_segment_t* segment,
                                     NR_EXECUTE_PROTO) {
  volatile int zcaught = 0;

  if (wraprec && wraprec->special_instrumentation_before) {
    zend_try {
      wraprec->special_instrumentation_before(wraprec, segment,
                                              NR_EXECUTE_ORIG_ARGS);
    }
    zend_catch { zcaught = 1; }
    zend_end_try();
  }

  return zcaught;
}
#endif
#endif
int nr_zend_call_orig_execute_special(nruserfn_t* wraprec,
                                      nr_segment_t* segment,
                                      NR_EXECUTE_PROTO TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO /* PHP8+ */
  (void)segment;
#endif
  volatile int zcaught = 0;
  NR_UNUSED_FUNC_RETURN_VALUE;
  zend_try {
    if (wraprec && wraprec->special_instrumentation) {
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO /* PHP8+ */
      wraprec->special_instrumentation(NR_EXECUTE_ORIG_ARGS);
#else
      wraprec->special_instrumentation(wraprec, segment,
                                       NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
#endif
    } else {
      NR_PHP_PROCESS_GLOBALS(orig_execute)
      (NR_EXECUTE_ORIG_ARGS_OVERWRITE TSRMLS_CC);
    }
  }
  zend_catch { zcaught = 1; }
  zend_end_try();
  return zcaught;
}

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
/* Hashmap with pointers to wraprecs. Some, that are re-usable between requests,
 * are stored in linked list. These wraprecs are created once per interesting
 * function detection, and destroyed at module shutdown. Some, that are
 * transient and not re-usable between requests, are not stored in linked list.
 * Transient wraprecs are created on the fly and destroyed at request shutdown.
 * However wrapping is done the same way for both types of wraprecs and happens
 * once per each request, i.e. for each request the hashmap is created anew,
 * when user function is instrumented, its wraprec is added to the hashmap, and
 * at the end of the request the hashmap is destroyed together with transient
 * wraprecs. Re-usable wraprecs are not destroyed - they are re-set. */
static nr_php_wraprec_hashmap_t* user_function_wrappers;

static inline void nr_php_wraprec_lookup_set(nruserfn_t* wr,
                                             zend_function* zf) {
  nr_php_wraprec_hashmap_update(user_function_wrappers, zf, wr);
}
static inline nruserfn_t* nr_php_wraprec_lookup_get(zend_function* zf) {
  nruserfn_t* wraprec = NULL;

  nr_php_wraprec_hashmap_get_into(user_function_wrappers, zf, &wraprec);

  return wraprec;
}

/*
 * Init user instrumentation. This must only be called on request init!
 * This creates wraprec lookup hashmap and registers wraprec destructor
 * callback - reset_wraprec - which is called on request shutdown.
 */
static void reset_wraprec(nruserfn_t*);
void nr_php_init_user_instrumentation(void) {
  if (NULL != user_function_wrappers) {
    /* Should not happen */
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "user_function_wrappers lookup hashmap already initialized!");
    return;
  }
  user_function_wrappers
      = nr_php_wraprec_hashmap_create_buckets(1024, reset_wraprec);
}

/*
 * This callback resets user instrumentation. It is called at request shutdown
 * when user instrumentation is reset - lookup hashmap is destroyed together
 * with transient wraprecs and non-transient wraprecs are reset (mark as not
 * wrapped). This happens because with new request/transaction php is loading
 * all new user code.
 */
static void nr_php_user_wraprec_destroy(nruserfn_t** wraprec_ptr);
static void reset_wraprec(nruserfn_t* wraprec) {
  nruserfn_t* p = wraprec;
  nr_php_wraprec_hashmap_key_release(&p->key);
  if (p->is_transient) {
    nr_php_user_wraprec_destroy((nruserfn_t**)&wraprec);
  } else {
    p->is_wrapped = 0;
  }
}
#endif

/*
 * Wrap an existing user-defined (written in PHP) function with an
 * instrumentation function. Actually, what we do is just set a pointer in the
 * reserved resources section of the op_array: the pointer points to the
 * wraprec. Non-wrapped functions have a 0 pointer in that field and thus
 * the execution function can quickly determine whether a user-defined function
 * is instrumented.
 *
 * nr_php_wrap_user_function_internal is the usual function that is used;
 * nr_php_wrap_zend_function is available for situations where we don't want to
 * (or can't) match by name and have the zend_function available. (The main use
 * case for this is to allow instrumenting closures, but it's useful anywhere
 * we're dealing with a callable rather than a name.)
 *
 * There are two main structures containing `wraprecs`.
 * `nr_php_op_array_set_wraprec` is a list of pointers to `wraprecs` that will
 * contain all our custom instrumentation and all the user specified
 * instrumentation they want to monitor. `user_function_wrappers` is a vector of
 * pointers to wrappers, after the zend function represented by a wraprec has
 * the reserved field modified, the pointer to the wraprec (which again, exists
 * in `nr_php_op_array_set_wraprec`) goes into the vector.
 * `nr_php_op_array_set_wraprec` is always a superset of
 * `user_function_wrappers` and the wraprec pointers that exist in
 * `user_function_wrappers` always exist in `nr_php_op_array_set_wraprec`.
 *
 * `nr_php_op_array_set_wraprec` is populated a few different ways.
 * 1)from function `nr_php_add_transaction_naming_function` called from
 * `php_nrini.c` to set the naming for all the transactions the user  set in the
 * ini with `newrelic.webtransaction.name.functions` 2) from function
 * `nr_php_add_custom_tracer` from `php_nrini.c` to set the naming for all the
 * transactions the user set in the ini with
 * `newrelic.transaction_tracer.custom` 3)
 * nr_php_user_function_add_declared_callback (prior to PHP 7.3) 4) from
 * function `nr_php_wrap_user_function` called from php_wrapper sets the wraprec
 * with framework specific instrumentation. Optionally specifies transience.
   5) from function `nr_php_wrap_callable` (in `php_wrapper.c`) used only by
 * Wordpress and predis for custom instrumentation that sets
 * `is_transient`.
 *
 * Transient wrappers get disposed of at the end of each request at RSHUTDOWN lifecycle.
 *
 * When overwriting the zend_execute_ex function, every effort was made to
 * reduce performance overhead because until the agent returns control, we are
 * the bottleneck of PHP execution on a customer's machine. Overwriting the
 * reserved field was seen as a quick way to check if a function is instrumented
 * or not.
 *
 * However, with PHP 8+, we've begun noticing more conflicts with the reserved
 * fields.  Additionally, as we are no longer halting execution while we
 * process, we can search through `nr_php_op_array_set_wraprec` instead of
 * setting reserved field and getting from the `user_function_wrappers` vector.
 * This stops the issues (segfaults, incorrect naming in laravel, etc) that we
 * were observing, especially with PHP 8.1.
 */
static void nr_php_wrap_zend_function(zend_function* func,
                                      nruserfn_t* wraprec TSRMLS_DC) {
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  nr_php_wraprec_lookup_set(wraprec, func);
#else
  nr_php_op_array_set_wraprec(&func->op_array, wraprec TSRMLS_CC);
#endif
  wraprec->is_wrapped = 1;

  if (wraprec->declared_callback) {
    (wraprec->declared_callback)(TSRMLS_C);
  }
}

static void nr_php_wrap_user_function_internal(nruserfn_t* wraprec TSRMLS_DC) {
  zend_function* orig_func = 0;

  if (0 == NR_PHP_PROCESS_GLOBALS(done_instrumentation)) {
    return;
  }

  if (wraprec->is_wrapped) {
    return;
  }

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    && defined OVERWRITE_ZEND_EXECUTE_DATA /* PHP8+ */
  if (nrunlikely(-1 == NR_PHP_PROCESS_GLOBALS(zend_offset))) {
    return;
  }
#endif
  if (0 == wraprec->classname) {
    orig_func = nr_php_find_function(wraprec->funcnameLC TSRMLS_CC);
  } else {
    zend_class_entry* orig_class = 0;

    orig_class = nr_php_find_class(wraprec->classnameLC TSRMLS_CC);
    orig_func = nr_php_find_class_method(orig_class, wraprec->funcnameLC);
  }

  if (NULL == orig_func) {
    /* It could be in a file not yet loaded, no reason to log anything. */
    return;
  }
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  wraprec->func = orig_func;
#endif

  if (ZEND_USER_FUNCTION != orig_func->type) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s%s%s is not a user function",
                     wraprec->classname ? wraprec->classname : "",
                     wraprec->classname ? "::" : "", wraprec->funcname);

    /*
     * Prevent future wrap attempts for performance and to prevent spamming the
     * logs with this message.
     */
    wraprec->is_disabled = 1;
    return;
  }
  nr_php_wrap_zend_function(orig_func, wraprec TSRMLS_CC);
}

static nruserfn_t* nr_php_user_wraprec_create(void) {
  return (nruserfn_t*)nr_zalloc(sizeof(nruserfn_t));
}

static nruserfn_t* nr_php_user_wraprec_create_named(const char* full_name,
                                                    int full_name_len) {
  int i;
  const char* name;
  const char* klass;
  int name_len;
  int klass_len;
  nruserfn_t* wraprec;

  if (0 == full_name) {
    return 0;
  }
  if (full_name_len <= 0) {
    return 0;
  }

  name = full_name;
  name_len = full_name_len;
  klass = 0;
  klass_len = 0;

  /* If class::method, then break into two strings */
  for (i = 0; i < full_name_len; i++) {
    if ((':' == full_name[i]) && (':' == full_name[i + 1])) {
      klass = full_name;
      klass_len = i;
      name = full_name + i + 2;
      name_len = full_name_len - i - 2;
    }
  }

  /* Create the wraprecord */
  wraprec = nr_php_user_wraprec_create();
  wraprec->funcname = nr_strndup(name, name_len);
  wraprec->funcnamelen = name_len;
  wraprec->funcnameLC = nr_string_to_lowercase(wraprec->funcname);

  if (klass) {
    wraprec->classname = nr_strndup(klass, klass_len);
    wraprec->classnamelen = klass_len;
    wraprec->classnameLC = nr_string_to_lowercase(wraprec->classname);
    wraprec->is_method = 1;
  }

  wraprec->supportability_metric = nr_txn_create_fn_supportability_metric(
      wraprec->funcname, wraprec->classname);

  return wraprec;
}

static void nr_php_user_wraprec_destroy(nruserfn_t** wraprec_ptr) {
  nruserfn_t* wraprec;

  if (0 == wraprec_ptr) {
    return;
  }
  wraprec = *wraprec_ptr;
  if (0 == wraprec) {
    return;
  }

  nr_free(wraprec->supportability_metric);
  nr_free(wraprec->drupal_module);
  nr_free(wraprec->drupal_hook);
  nr_free(wraprec->classname);
  nr_free(wraprec->funcname);
  nr_free(wraprec->classnameLC);
  nr_free(wraprec->funcnameLC);
  nr_realfree((void**)wraprec_ptr);
}

static int nr_php_user_wraprec_is_match(const nruserfn_t* w1,
                                        const nruserfn_t* w2) {
  if ((0 == w1) && (0 == w2)) {
    return 1;
  }
  if ((0 == w1) || (0 == w2)) {
    return 0;
  }
  if (0 != nr_strcmp(w1->funcnameLC, w2->funcnameLC)) {
    return 0;
  }
  if (0 != nr_strcmp(w1->classnameLC, w2->classnameLC)) {
    return 0;
  }
  return 1;
}

static void nr_php_add_custom_tracer_common(nruserfn_t* wraprec) {
  /* Add the wraprecord to the list. */
  wraprec->next = nr_wrapped_user_functions;
  nr_wrapped_user_functions = wraprec;
}

#define NR_PHP_UNKNOWN_FUNCTION_NAME "{unknown}"
nruserfn_t* nr_php_add_custom_tracer_callable(zend_function* func TSRMLS_DC) {
  char* name = NULL;
  nruserfn_t* wraprec;

  if ((NULL == func) || (ZEND_USER_FUNCTION != func->type)) {
    return NULL;
  }

  /*
   * For logging purposes, let's create a name if we're logging at verbosedebug.
   */
  if (nrl_should_print(NRL_VERBOSEDEBUG, NRL_INSTRUMENT)) {
    name = nr_php_function_debug_name(func);
  }

  /*
   * nr_php_op_array_get_wraprec does basic sanity checks on the stored
   * wraprec.
   */
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  wraprec = nr_php_op_array_get_wraprec(&func->op_array TSRMLS_CC);
#else
  wraprec = nr_php_wraprec_lookup_get(func);
#endif

  if (wraprec) {
    nrl_verbosedebug(NRL_INSTRUMENT, "reusing custom wrapper for callable '%s'",
                     name);
    nr_free(name);
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
    nr_php_observer_remove_begin_handler(func, wraprec);
    nr_php_observer_remove_end_handler(func, wraprec);
#endif
    return wraprec;
  }

  wraprec = nr_php_user_wraprec_create();
  wraprec->is_transient = true;

  nrl_verbosedebug(NRL_INSTRUMENT, "adding custom for callable '%s'", name);
  nr_free(name);

  nr_php_wrap_zend_function(func, wraprec TSRMLS_CC);
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  nr_php_add_custom_tracer_common(wraprec);
#endif
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  nr_php_observer_remove_begin_handler(func, NULL);
  nr_php_observer_remove_end_handler(func, NULL);
#endif

  return wraprec;
}

nruserfn_t* nr_php_add_custom_tracer_named(const char* namestr,
                                           size_t namestrlen) {
  nruserfn_t* wraprec;
  nruserfn_t* p;

  wraprec = nr_php_user_wraprec_create_named(namestr, namestrlen);
  if (0 == wraprec) {
    return 0;
  }

  /* Make sure that we are not duplicating an existing wraprecord */
  p = nr_wrapped_user_functions;

  while (0 != p) {
    if (nr_php_user_wraprec_is_match(p, wraprec)) {
      nrl_verbosedebug(
          NRL_INSTRUMENT,
          "reusing custom wrapper for '" NRP_FMT_UQ "%.5s" NRP_FMT_UQ "'",
          NRP_PHP(wraprec->classname),
          (0 == wraprec->classname) ? "" : "::", NRP_PHP(wraprec->funcname));

      nr_php_user_wraprec_destroy(&wraprec);
      nr_php_wrap_user_function_internal(p TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
      nr_php_observer_remove_begin_handler(p->func, wraprec);
      nr_php_observer_remove_end_handler(p->func, wraprec);
#endif
      return p; /* return the wraprec we are duplicating */
    }
    p = p->next;
  }

  nrl_verbosedebug(
      NRL_INSTRUMENT, "adding custom for '" NRP_FMT_UQ "%.5s" NRP_FMT_UQ "'",
      NRP_PHP(wraprec->classname),
      (0 == wraprec->classname) ? "" : "::", NRP_PHP(wraprec->funcname));

  nr_php_wrap_user_function_internal(wraprec TSRMLS_CC);
  /* non-transient wraprecs are added to both the hashmap and linked list.
   * At request shutdown, the hashmap will free transients, but leave
   * non-transients to be freed when the linked list is disposed of which is at
   * module shutdown */
  nr_php_add_custom_tracer_common(wraprec);
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  nr_php_observer_remove_begin_handler(wraprec->func, NULL);
  nr_php_observer_remove_end_handler(wraprec->func, NULL);
#endif

  return wraprec; /* return the new wraprec */
}

/*
 * Reset the user instrumentation records because we're starting a new
 * transaction and so we'll be loading all new user code.
 *
 * For PHP 7.4+ this function is called on request shutdown to release memory
 * allocated for lookup hashmap! Additionally hashmap's value destructor
 * callback will reset all non-transient wraprecs (mark them as not wrapped),
 * destroy all transient wraprecs.
 *
 */
void nr_php_reset_user_instrumentation(void) {
#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
  // send a metric with the number of transient wrappers
  if (NULL != user_function_wrappers) {
    nr_php_wraprec_hashmap_stats_t stats
        = nr_php_wraprec_hashmap_destroy(&user_function_wrappers);

    nrl_debug(NRL_INSTRUMENT, "# elements: %lu, # buckets used: %lu",
              stats.elements, stats.buckets_used);
    nrl_debug(NRL_INSTRUMENT, "collisions - min: %lu, max: %lu, avg: %lu",
              stats.collisions_min, stats.collisions_max,
              stats.collisions_mean);
  }
#else
  nruserfn_t* p = nr_wrapped_user_functions;
  while (0 != p) {
    p->is_wrapped = 0;
    p = p->next;
  }
#endif
}

/*
 * Remove any transient wraprecs. This must only be called on request shutdown!
 */
void nr_php_remove_transient_user_instrumentation(void) {
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  nruserfn_t* p = nr_wrapped_user_functions;
  nruserfn_t* prev = NULL;

  while (NULL != p) {
    if (p->is_transient) {
      nruserfn_t* trans = p;

      if (prev) {
        prev->next = p->next;
      } else {
        nr_wrapped_user_functions = p->next;
      }

      p = p->next;
      nr_php_user_wraprec_destroy(&trans);
    } else {
      prev = p;
      p = p->next;
    }
  }
#endif
}

/*
 * Wrap all the interesting user functions with instrumentation.
 */
void nr_php_add_user_instrumentation(TSRMLS_D) {
  nruserfn_t* p = nr_wrapped_user_functions;

  while (0 != p) {
    if ((0 == p->is_wrapped) && (0 == p->is_disabled)) {
      nr_php_wrap_user_function_internal(p TSRMLS_CC);
    }
    p = p->next;
  }
}

void nr_php_add_transaction_naming_function(const char* namestr,
                                            int namestrlen TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_named(namestr, namestrlen);

  if (NULL != wraprec) {
    wraprec->is_names_wt_simple = 1;
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
    nr_php_observer_add_begin_handler(wraprec->func, wraprec);
    nr_php_observer_add_end_handler(wraprec->func, wraprec);
#endif
  }
}

void nr_php_add_custom_tracer(const char* namestr, int namestrlen TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_named(namestr, namestrlen);

  if (NULL != wraprec) {
    wraprec->create_metric = 1;
    wraprec->is_user_added = 1;
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
    nr_php_observer_add_begin_handler(wraprec->func, wraprec);
    nr_php_observer_add_end_handler(wraprec->func, wraprec);
#endif
  }
}

void nr_php_add_exception_function(zend_function* func TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_callable(func TSRMLS_CC);

  if (wraprec) {
    wraprec->is_exception_handler = 1;
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
    nr_php_observer_add_begin_handler(func, wraprec);
    nr_php_observer_add_end_handler(func, wraprec);
#endif
  }
}

void nr_php_remove_exception_function(zend_function* func TSRMLS_DC) {
  nruserfn_t* wraprec;

  if ((NULL == func) || (ZEND_USER_FUNCTION != func->type)) {
    return;
  }

#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  wraprec = nr_php_op_array_get_wraprec(&func->op_array TSRMLS_CC);
#else
  wraprec = nr_php_wraprec_lookup_get(func);
#endif
  if (wraprec) {
    wraprec->is_exception_handler = 0;
  }
}

void nr_php_destroy_user_wrap_records(void) {
  nruserfn_t* next_user_wraprec;

  next_user_wraprec = nr_wrapped_user_functions;
  while (next_user_wraprec) {
    nruserfn_t* wraprec = next_user_wraprec;

    next_user_wraprec = wraprec->next;
    nr_php_user_wraprec_destroy(&wraprec);
  }

  nr_wrapped_user_functions = NULL;
}

/*
 * This is a similar list, but for the dynamically added user-defined functions
 * rather than the statically defined internal/binary functions above.
 */
nruserfn_t* nr_wrapped_user_functions = 0;

void nr_php_user_function_add_declared_callback(const char* namestr,
                                                int namestrlen,
                                                nruserfn_declared_t callback
                                                    TSRMLS_DC) {
  nruserfn_t* wraprec = nr_php_add_custom_tracer_named(namestr, namestrlen);

  if (0 != wraprec) {
    wraprec->declared_callback = callback;

    /*
     * Immediately fire the callback if the function is already wrapped.
     */
    if (wraprec->is_wrapped && callback) {
      (callback)(TSRMLS_C);
    }
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
    nr_php_observer_add_begin_handler(wraprec->func, wraprec);
    nr_php_observer_add_end_handler(wraprec->func, wraprec);
#endif
  }
}

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO
nruserfn_t* nr_php_get_wraprec(zend_function* zf) {
  return nr_php_wraprec_lookup_get(zf);
}
#else
/*
 * The functions nr_php_op_array_set_wraprec and
 * nr_php_op_array_get_wraprec set and retrieve pointers to function wrappers
 * (wraprecs) stored in the oparray of zend functions.
 *
 * There's the danger that other PHP modules or even other PHP processes
 * overwrite those pointers. We try to detect that by validating the
 * stored pointers.
 *
 * Since PHP 7.3, OpCache stores functions and oparrays in shared
 * memory. Consequently, the wraprec pointers we store in the oparray
 * might be overwritten by other processes. Dereferencing an overwritten
 * wraprec pointer will most likely cause a crash.
 *
 * The remedy, applied for all PHP versions:
 *
 *  1. All wraprec pointers are stored in a global vector.
 *
 *  2. The index of the wraprec pointer in the vector is mangled with
 *     the current process id. This results in a value with the lower 16 bits
 *     holding the vector index (i) and the higher bits holding the process
 *     id (p):
 *
 *       0xppppiiii (32 bit)
 *       0xppppppppppppiiii (64 bit)
 *
 *     This supports a maximum of 65536 instrumented functions.
 *
 *  3. This mangled value is stored in the oparray.
 *
 *  4. When a zend function is called and the agent tries to obtain the
 *     wraprec, the upper bits of the value are compared to the current process
 *     id. If they match, the index in the lower 16 bits is considered safe and
 *     is used. Otherwise the function is considered as uninstrumented.
 */

void nr_php_op_array_set_wraprec(zend_op_array* op_array,
                                 nruserfn_t* func TSRMLS_DC) {
  uintptr_t index;

  if (NULL == op_array || NULL == func) {
    return;
  }

  if (!nr_vector_push_back(NRPRG(user_function_wrappers), func)) {
    return;
  }

  index = nr_vector_size(NRPRG(user_function_wrappers)) - 1;

  index |= (NRPRG(pid) << 16);

  op_array->reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)] = (void*)index;
}

nruserfn_t* nr_php_op_array_get_wraprec(
    const zend_op_array* op_array TSRMLS_DC) {
  uintptr_t index;
  uint64_t pid;

  if (nrunlikely(NULL == op_array)) {
    return NULL;
  }

  index = (uintptr_t)op_array->reserved[NR_PHP_PROCESS_GLOBALS(zend_offset)];

  if (0 == index) {
    return NULL;
  }

  pid = index >> 16;
  index &= 0xffff;

  if (pid != NRPRG(pid)) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Skipping instrumented function: pid mismatch, got " NR_INT64_FMT
        ", expected " NR_INT64_FMT,
        pid, NRPRG(pid));
    return NULL;
  }

  return (nruserfn_t*)nr_vector_get(NRPRG(user_function_wrappers), index);
}
#endif /* PHP < 7.4 */
