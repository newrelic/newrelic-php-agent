/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file declares API functions for internal use only.
 */
#ifndef PHP_API_INTERNAL_HDR
#define PHP_API_INTERNAL_HDR

/*
 * Proto   : array newrelic_get_request_metadata ([string $transport =
 *           'unknown'])
 *
 * Returns : An array of header key-value pairs that should be added to
 *           outbound requests for CAT.
 */
extern PHP_FUNCTION(newrelic_get_request_metadata);

#ifdef ENABLE_TESTING_API

/*
 * Proto   : string newrelic_get_hostname ()
 *
 * Returns : The hostname as returned by nr_system_get_hostname().
 */
extern PHP_FUNCTION(newrelic_get_hostname);

/*
 * Proto   : array newrelic_get_metric_table ([bool $scoped = false])
 *
 * Params  : 1. True to return scoped metrics; false to return unscoped
 *              metrics.
 *
 * Returns : The metric table, as decoded from its JSON serialised form.
 */
extern PHP_FUNCTION(newrelic_get_metric_table);

/*
 * Proto   : array newrelic_get_slowsqls ()
 *
 * Returns : An array of slowsqls. Each slowsql is an array with the following
 *           keys: id, count, min, max, total, metric, query, params.
 */
extern PHP_FUNCTION(newrelic_get_slowsqls);

/*
 * Proto   : string newrelic_get_trace_json ()
 *
 * Returns : The transaction trace JSON that would be sent to the daemon if the
 *           transaction ended at the point the function is called. This string
 *           is owned by the caller.
 */
extern PHP_FUNCTION(newrelic_get_trace_json);

/*
 * Proto   : string newrelic_get_error_json ()
 *
 * Returns : The error trace JSON that would be sent to the daemon if the
 *           transaction ended at the point the function is called. This string
 *           is owned by the caller.
 */
extern PHP_FUNCTION(newrelic_get_error_json);

/*
 * Proto   : string newrelic_get_transaction_guid ()
 *
 * Returns : The transaction guid.
 */
extern PHP_FUNCTION(newrelic_get_transaction_guid);

/*
 * Proto   : bool newrelic_is_localhost (string $host)
 *
 * Params  : 1. The host name to check with
 *              nr_datastore_instance_is_localhost().
 *
 * Returns : True if the host name is the local host; false otherwise.
 */
extern PHP_FUNCTION(newrelic_is_localhost);

/*
 * Proto   : bool newrelic_is_recording ()
 *
 * Returns : True if the agent is recording; false otherwise.
 */
extern PHP_FUNCTION(newrelic_is_recording);

#endif /* ENABLE_TESTING_API */

#endif /* PHP_API_INTERNAL_HDR */
