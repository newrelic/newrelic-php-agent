/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for metric table management.
 *
 * Manage the creation, manipulation and destruction of a metrics table. These
 * tables are used in both the parent process for the aggregated set of metrics
 * for each individual agent, as well as in each agent during a transaction.
 * The actual internal representation of the table is hidden from the caller,
 * which should only use the access functions provided here for manipulating
 * the table.
 */
#ifndef UTIL_METRICS_HDR
#define UTIL_METRICS_HDR

#include "util_time.h"

/*
 * This is the default maximum number of metrics for a metric table (after
 * which only forced metrics will be added).
 */
#define NR_METRIC_DEFAULT_LIMIT 2000

typedef struct _nrminttable_t nrmtable_t;
typedef struct _nrmintmetric_t nrmetric_t;

/* Possible flags settings */
#define MET_IS_APDEX 0x00000001
#define MET_FORCED 0x00000002

/*
 * Purpose : Create a new metric table.
 *
 * Params  : 1. The maximum size of the table.  After this number of metrics
 *              has been reached, only forced metrics will be added.
 */
extern nrmtable_t* nrm_table_create(int max_size);

/*
 * Purpose : Destroys a metric table, freeing all of its associated memory.
 */
extern void nrm_table_destroy(nrmtable_t** table_p);

/*
 * Purpose : Find a metric in a table.  Returns NULL if the metric is not found.
 */
extern nrmetric_t* nrm_find(nrmtable_t* table, const char* name);

/*
 * Purpose : Add metrics to a table with varying amount of data.  The _force_
 *           versions will add the metric even if the table's max_size has
 *           been reached.
 */
extern void nrm_add_ex(nrmtable_t* table,
                       const char* name,
                       nrtime_t duration,
                       nrtime_t exclusive);
extern void nrm_force_add_ex(nrmtable_t* table,
                             const char* name,
                             nrtime_t duration,
                             nrtime_t exclusive);
extern void nrm_add(nrmtable_t* table, const char* name, nrtime_t duration);
extern void nrm_force_add(nrmtable_t* table,
                          const char* name,
                          nrtime_t duration);

extern void nrm_add_apdex(nrmtable_t* table,
                          const char* name,
                          nrtime_t satisfying,
                          nrtime_t tolerating,
                          nrtime_t failing,
                          nrtime_t apdex);
extern void nrm_force_add_apdex(nrmtable_t* table,
                                const char* name,
                                nrtime_t satisfying,
                                nrtime_t tolerating,
                                nrtime_t failing,
                                nrtime_t apdex);

/*
 * Purpose : Add a metric: These function allow for full control over the data
 *           fields of an added metric.
 */
extern void nrm_add_internal(int force,
                             nrmtable_t* table,
                             const char* name,
                             nrtime_t count,
                             nrtime_t total,
                             nrtime_t exclusive,
                             nrtime_t min,
                             nrtime_t max,
                             nrtime_t sum_of_squares);
extern void nrm_add_apdex_internal(int force,
                                   nrmtable_t* table,
                                   const char* name,
                                   nrtime_t satisfying,
                                   nrtime_t tolerating,
                                   nrtime_t failing,
                                   nrtime_t min_apdex,
                                   nrtime_t max_apdex);

/*
 * Purpose : Duplicate a metric with a new name.  The source metric is
 *           unmodified.  This function has no effect if the source metric is
 *           not found.
 */
extern void nrm_duplicate_metric(nrmtable_t* table,
                                 const char* current_name,
                                 const char* new_name);

/*
 * Purpose : Get the current table size.
 */
extern int nrm_table_size(const nrmtable_t* tp);

/*
 * Purpose : Get a metric in the table.
 */
extern const nrmetric_t* nrm_get_metric(const nrmtable_t* table, int i);

/*
 * Purpose : Acquire the name or data of a metric.
 */
extern const char* nrm_get_name(const nrmtable_t* table, const nrmetric_t* met);
extern int nrm_is_apdex(const nrmetric_t* metric);
extern int nrm_is_forced(const nrmetric_t* metric);
extern nrtime_t nrm_satisfying(const nrmetric_t* metric);
extern nrtime_t nrm_tolerating(const nrmetric_t* metric);
extern nrtime_t nrm_failing(const nrmetric_t* metric);
extern nrtime_t nrm_count(const nrmetric_t* metric);
extern nrtime_t nrm_total(const nrmetric_t* metric);
extern nrtime_t nrm_exclusive(const nrmetric_t* metric);
extern nrtime_t nrm_min(const nrmetric_t* metric);
extern nrtime_t nrm_max(const nrmetric_t* metric);
extern nrtime_t nrm_sumsquares(const nrmetric_t* metric);

/*
 * Purpose : Turn a metric table into the JSON format expected by the daemon.
 *           Returns NULL on error.
 */
extern char* nr_metric_table_to_daemon_json(const nrmtable_t* table);

#endif /* UTIL_METRICS_HDR */
