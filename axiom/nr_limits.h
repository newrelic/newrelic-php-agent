/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This header contains default limits.
 *
 * This should be kept in sync with src/newrelic/limits/limits.go, which is the
 * daemon's view of the default limits.
 */
#ifndef NR_LIMITS_HDR
#define NR_LIMITS_HDR

/*
 * The default maximum number of transaction events in a harvest cycle.
 */
#define NR_MAX_ANALYTIC_EVENTS 10000

/*
 * The default maximum number of custom events in a transaction.
 */
#define NR_DEFAULT_CUSTOM_EVENTS_MAX_SAMPLES_STORED 30000

/*
 * The maximum number of custom events in a transaction.
 */
#define NR_MAX_CUSTOM_EVENTS_MAX_SAMPLES_STORED 100000

/*
 * Set the maximum number of errors in a transaction.
 */
#define NR_MAX_ERRORS 20

/*
 * The maximum number of segments in a transaction.
 */
#define NR_MAX_SEGMENTS 2000

/*
 * The default maximum number of span events in a transaction.
 */
#define NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED 2000

/*
 * The absolute maximum number of span events in a transaction.
 */
#define NR_MAX_SPAN_EVENTS_MAX_SAMPLES_STORED 10000

/*
 * The maximum number of span events in an 8T span batch.
 */
#define NR_MAX_8T_SPAN_BATCH_SIZE 1000

/*
 * The default maximum number of log events in a transaction.
 */
#define NR_DEFAULT_LOG_EVENTS_MAX_SAMPLES_STORED 10000

/*
 * The maximum number of log events in a transaction.
 */
#define NR_MAX_LOG_EVENTS_MAX_SAMPLES_STORED 20000

#endif /* NR_LIMITS_HDR */
