/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains functions pertaining to handling PHP errors and
 * exceptions.
 */
#ifndef PHP_ERROR_HDR
#define PHP_ERROR_HDR

#include "zend.h"

/*
 * Constants for defined error priorities.
 *
 * NR_PHP_ERROR_PRIORITY_API_PRIORITIZED is the priority that will be used when
 * newrelic_notice_error() is called and
 * newrelic.error_collector.prioritize_api_errors is enabled in php.ini. This
 * needs to be higher than any organic value that nr_php_error_get_priority()
 * can return.
 *
 * NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION is the priority that will be used
 * for uncaught exceptions. This should be higher than
 * NR_PHP_ERROR_PRIORITY_API_PRIORITIZED to ensure that uncaught exceptions
 * "win", since we only support one error per transaction, and the uncaught
 * exception is what the user really needs to see in that case (after all, it
 * resulted in the transaction's ultimate failure!).
 */
#define NR_PHP_ERROR_PRIORITY_API_PRIORITIZED 99
#define NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION 100

typedef enum _nr_php_exception_action_t {
  NR_PHP_EXCEPTION_FILTER_REPORT, /* create a traced error */
  NR_PHP_EXCEPTION_FILTER_IGNORE, /* do not create a traced error */
} nr_php_exception_action_t;

/*
 * Purpose : Determine whether an exception should be ignored or reported
 *           as a traced error.
 *
 * Params  : 1. A pointer to the exception.
 *
 * Returns : One of the nr_php_exception_action_t values.
 */
typedef nr_php_exception_action_t (*nr_php_exception_filter_fn)(
    zval* exception TSRMLS_DC);

/*
 * Purpose : Initialize an exception filter chain.
 *
 * Params  : 1. The exception filter chain to initialize.
 */
extern void nr_php_exception_filters_init(zend_llist* chain);

/*
 * Purpose : Free any resources associated with an exception filter chain.
 *
 * Params  : 1. The exception filter chain to destroy.
 */
extern void nr_php_exception_filters_destroy(zend_llist* chain);

/*
 * Purpose : Add an exception filter to an exception filter chain.
 *
 * Params  : 1. The exception filter chain to modify.
 *           2. The exception filter function to add.
 *
 * Notes   : Exception filters must be installed prior to the exception
 *           being thrown to have any effect.
 */
extern nr_status_t nr_php_exception_filters_add(zend_llist* chain,
                                                nr_php_exception_filter_fn fn);

/*
 * Purpose : Remove an exception filter from an exception filter chain.
 *
 * Params  : 1. The exception filter chain to modify.
 *           2. The exception filter function to remove.
 */
extern nr_status_t nr_php_exception_filters_remove(
    zend_llist* chain,
    nr_php_exception_filter_fn fn);

/*
 * Purpose : Determine whether an exception should be ignored or reported as a
 *           traced error based on the value of the
 *           newrelic.error_collector.ignore_exceptions setting.
 *
 * Params  : 1. The exception to filter.
 *
 * Returns : One of the nr_php_exception_action_t values.
 */
extern nr_php_exception_action_t nr_php_ignore_exceptions_ini_filter(
    zval* exception TSRMLS_DC);

/*
 * Our PHP-visible function that we install at the bottom of the user exception
 * handler stack to notice uncaught exceptions and then generate an error
 * similar to the one that PHP itself generates so that any user logging still
 * occurs.
 */
extern PHP_FUNCTION(newrelic_exception_handler);

/*
 * Purpose : Converts a PHP error type into an error priority, which we then
 *           use to determine which error should be sent when a transaction
 *           ends.
 *
 * Params  : 1. The error type.
 *
 * Returns : The priority. Higher numbers indicate higher priority errors.
 */
extern int nr_php_error_get_priority(int type);

/*
 * Purpose : Install newrelic_exception_handler as the user exception handler
 *           in PHP.
 */
extern void nr_php_error_install_exception_handler(TSRMLS_D);

/*
 * Purpose : Record an error for the given exception in a transaction.
 *
 * Params  : 1. The transaction to record the error in.
 *           2. The exception to record an error for.
 *           3. The error priority to use.
 *           4. Whether we want to add the error to the current segment.
 *           5. A prefix to prepend to the error message before the class name.
 *              If NULL, then the default "Exception " will be used.
 *           6. The exception filters to apply.
 *              Typically, &NRPRG (exception_filters) or NULL to disable
 *              exception filtering.
 *
 * Returns : NR_SUCCESS if an error was recorded; NR_FAILURE otherwise (which
 *           will generally indicate that the exception wasn't really an
 *           exception).
 */
extern nr_status_t nr_php_error_record_exception(nrtxn_t* txn,
                                                 zval* exception,
                                                 int priority,
                                                 bool add_to_current_segment,
                                                 const char* prefix,
                                                 zend_llist* filters TSRMLS_DC);

/*
 * Purpose : Record an uncaught exception on a segment that exits.
 *
 * Params  : 1. The transaction.
 *           2. The exception to record an error for.
 *           5. The exception filters to apply.
 *              Typically, &NRPRG (exception_filters) or NULL to disable
 *              exception filtering.
 *
 * Returns : NR_SUCCESS if an error was recorded; NR_FAILURE otherwise
 */
extern nr_status_t nr_php_error_record_exception_segment(nrtxn_t* txn,
                                                         zval* exception,
                                                         zend_llist* filters
                                                             TSRMLS_DC);
/*
 * Purpose : Check if the given zval is a valid exception.
 *
 * Params  : 1. The zval to check.
 *
 * Returns : Non-zero if the zval is an exception; zero otherwise.
 */
extern int nr_php_error_zval_is_exception(zval* zv TSRMLS_DC);

#endif /* PHP_ERROR_HDR */
