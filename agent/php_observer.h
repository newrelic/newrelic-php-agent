/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file is the wrapper for PHP 8+ Observer API (OAPI) functionality.
 *
 * The registered function handlers are the entry points of instrumentation and
 * are implemented in php_execute.c which contains the brains/helper functions
 * required to monitor PHP.
 */

#ifndef NEWRELIC_PHP_AGENT_PHP_OBSERVER_H
#define NEWRELIC_PHP_AGENT_PHP_OBSERVER_H

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */

#include "Zend/zend_observer.h"
#include "php_user_instrument.h"

/*
 * Purpose: There are a few various places, aside from the php_execute_* family
 * that will call NR_PHP_PROCESS_GLOBALS(orig_execute) so make it a noop to
 * handle all cases.
 *
 * Params:  NR_EXECUTE_PROTO_OVERWRITE which is not used.
 *
 * Returns : Void
 */
extern void nr_php_observer_no_op(zend_execute_data* execute_data NRUNUSED);

/*
 * Purpose : Register the OAPI function handlers and any other minit actions.
 *
 * Params  : None
 *
 * Returns : Void.
 */
void nr_php_observer_minit();

/*
 * Purpose : Call the necessary functions needed to instrument a function by
 *           updating a transaction or segment for a function that has just
 * started.  This function is registered via the Observer API and will be called
 * by the zend engine every time a function begins.  The zend engine directly
 * provides the zend_execute_data which has all details we need to know about
 * the function. This and nr_php_execute_observer_fcall_end sum to provide all
 * the functionality of nr_php_execute and nr_php _execute_enabled and as such
 * will use all the helper functions they also used.
 *
 *
 * Params  : 1. zend_execute_data: everything we need to know about the
 * function.
 *
 * Returns : Void.
 */
void nr_php_observer_fcall_begin(zend_execute_data* execute_data);
/*
 * Purpose : Call the necessary functions needed to instrument a function when
 *           updating a transaction or segment for a function that has just
 * ended. This function is registered via the Observer API and will be called by
 * the zend engine every time a function ends.  The zend engine directly
 * provides the zend_execute_data and the return_value pointer, both of which
 * have all details that the agent needs to know about the function. This and
 * nr_php_execute_observer_fcall_start sum to provide all the functionality of
 * nr_php_execute and nr_php_execute_enabled and as such will use all the helper
 * functions they also used.
 *
 *
 * Params  : 1. zend_execute_data: everything to know about the function.
 *           2. return_value: function return value information
 *
 * Returns : Void.
 */
void nr_php_observer_fcall_end(zend_execute_data* execute_data,
                               zval* func_return_value);


#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
bool nr_php_observer_is_registered(zend_function* func);
bool nr_php_observer_remove_begin_handler(zend_function* func, nruserfn_t* wraprec);
bool nr_php_observer_remove_end_handler(zend_function* func, nruserfn_t* wraprec);
void nr_php_observer_add_begin_handler(zend_function* func, nruserfn_t* wraprec);
void nr_php_observer_add_end_handler(zend_function* func, nruserfn_t* wraprec);

/* 
 * These different forms of fcall_begin and fcall_end are needed to properly utilize
 * the fields in a wraprec without looking it up every call.
*/
void nr_php_observer_empty_fcall_begin(zend_execute_data* execute_data);
void nr_php_observer_fcall_begin_instrumented(zend_execute_data* execute_data);
void nr_php_observer_fcall_begin_name_transaction(zend_execute_data* execute_data);

void nr_php_observer_empty_fcall_end(zend_execute_data* execute_data,
                                     zval* func_return_value);
void nr_php_observer_fcall_begin_late(zend_execute_data* execute_data, nrtime_t txn_start_time, bool name_transaction);
void nr_php_observer_fcall_end_keep_segment(zend_execute_data* execute_data,
                                            zval* func_return_value);
void nr_php_observer_fcall_end_late(zend_execute_data* execute_data, bool create_metric, nrtime_t txn_start_time);
void nr_php_observer_fcall_end_create_metric(zend_execute_data* execute_data,
                                             zval* func_return_value);
void nr_php_observer_fcall_end_exception_handler(zend_execute_data* execute_data,
                                                 zval* func_return_value);
#endif /* PHP 8.2+ */
#endif /* PHP8+ */

#endif  // NEWRELIC_PHP_AGENT_PHP_OBSERVER_H
