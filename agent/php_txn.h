/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_TXN_HDR
#define PHP_TXN_HDR

#include "php_newrelic.h"
#include "nr_txn.h"

/*
 * Returns whether a particular security policy feature is considered
 * secure or not according to the current client configuration. These
 * values are not the ultimate source of truth for whether a certain
 * security policy is enabled or not.  The agent sends these values to the
 * daemon for further calculation/consideration.
 */
bool nr_php_txn_is_policy_secure(const char* policy_name,
                                 const nrtxnopt_t* opts);

/*
 * Returns an object of supported policies
 *
 * We need to send the daemon a hash of the LASP policies we support.
 * This function returns those policies as an nro object with the
 * follow structure
 *
 * {"policy_name":{         //the policy name
 *    "supported": bool     //does the agent support this policy
 *    "enabled: bool        //does the policy seem enabled or
 *                          //disabled from the pov of the configuration
 *
 * @return An nrobj_t*, caller is responsible for freeing with nro_delete
 */
nrobj_t* nr_php_txn_get_supported_security_policy_settings();

/*
 * Purpose : Override the transaction name if PHP-FPM generated an error
 *           response internally.
 *
 * Params  : 1. The current transaction.
 */
extern void nr_php_txn_handle_fpm_error(nrtxn_t* txn TSRMLS_DC);

/*
 * Purpose : Create and record metrics for the PHP and agent versions.
 *
 * Params  : 1. The current transaction.
 *
 * Notes   : This function relies on NR_VERSION and the value of
 *           NRPRG(php_version) to create the metrics.
 */
extern void nr_php_txn_create_agent_php_version_metrics(nrtxn_t* txn);

/*
 * Purpose : Create and record metric for a specific agent version.
 *
 * Params  : 1. The current transaction.
 *
 * Notes   : This function relies on the value of the macro NR_VERSION
 *           to create.
 */
extern void nr_php_txn_create_agent_version_metric(nrtxn_t* txn);

/*
 * Purpose : Create and record metric for a specific PHP version.
 *
 * Params  : 1. The current transaction.
 *           2. The PHP agent version.
 */
extern void nr_php_txn_create_php_version_metric(nrtxn_t* txn,
                                                 const char* version);

/*
 * Purpose : Callback for nr_php_packages_iterate to create major
 *           version metrics.
 *
 * Params  : 1. PHP suggestion package version
 *           2. PHP suggestion package name
 *           3. PHP suggestion package name length
 *           4. The current transaction (via userdata)
 */
extern void nr_php_txn_php_package_create_major_metric(void* value,
                                                       const char* key,
                                                       size_t key_len,
                                                       void* user_data);

/*
 * Purpose : Create and record metric for a package major versions.
 *
 * Params  : 1. The current transaction.
 */
extern void nr_php_txn_create_packages_major_metrics(nrtxn_t* txn);

/*
 * Purpose : Filter the labels hash to exclude any labels that are in the
 *           newrelic.application_logging.forwarding.labels.exclude list.
 *
 * Params  : 1. The labels hash to filter.
 *
 * Returns : A new hash containing the filtered labels.
 *           If no labels exist or all labels are excluded, then return NULL.
 *
 */

extern nrobj_t* nr_php_txn_get_log_forwarding_labels(nrobj_t* labels);

/*
 * Purpose : Get a pointer to the currently-executing segment for a given
 *           async context.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A pointer to the active segment for the current PHP context that a
 * function is executing in (either main context or a fiber context).
 * Note    : The NRPRG_SHARED(current_php_context) and
 * txn->current_async_context values are in sync most of the time, but it's
 * important we keep track of both and use where appropriate.  Most of the time
 * they will be the same.  It's very rare that they aren't, but we want to
 * ensure we cover that case.   NRPRG_SHARED(current_php_context) is independent
 * of txn and always tells what context (fiber/main) PHP is executing in. 
 * txn->current_async_context depends on the currently executing segment of the
 * txn and allows alignment with whatever segment is currently executing on the
 * txn.  Examples: We could be in a fiber with context
 * NRPRG_SHARED(current_php_context), but if we are creating a few additional
 * async segments for some async instrumentation we are doing then that would be
 * current txn context.  If a txn start/stop occurs, that will also change the
 * context state for the txn even though the current_php_context remains
 * unchanged.
 *
 * With PHP Fibers, the current_php_context switches frequently.  We need to be
 * able to retrieve the segment associated with a particular fiber and not just
 * the last one the txn was executing. Any function that is based on an
 * execution trigger (ex: OAPI func_begin/end or the overwriting of
 * zend_execute_ex) implementation that does not use stacked segments (OAPI
 * implementation) must use NRPRG_SHARED(current_php_context) to retrieve the
 * segment and ensure both segment alignment with current context and correct
 * context parenting. Begin and end func are dependent on fiber context since it
 * deals with initial creation and proper retrieval of a segment.
 * NRPRG_SHARED(current_php_context) will always tell the current fiber context
 * (whether fiber or main) that we need to create or get a segment. Note :
 * Callers wishing to get the segment currently executing in
 * the txn should use nr_txn_get_current_segment_txn_context.
 * Callers wishing to
 * get the current segment on a specific context should use
 * nr_txn_get_current_segment() to get the current segment on any other context
 * they are interested in.
 */
static inline nr_segment_t* nr_php_txn_get_current_segment_php_context(
    nrtxn_t* txn) {
  return nr_txn_get_current_segment(txn, NRPRG_SHARED(current_php_context));
}

#endif /* PHP_TXN_HDR */
