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
                               zval* return_value);
#endif /* PHP8+ */

#endif  // NEWRELIC_PHP_AGENT_PHP_OBSERVER_H
