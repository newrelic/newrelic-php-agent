/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains internal data structures for metrics.
 */
#ifndef UTIL_METRICS_PRIVATE_HDR
#define UTIL_METRICS_PRIVATE_HDR

#include <stdint.h>

#include "nr_axiom.h"
#include "util_metrics.h"
#include "util_string_pool.h"

/*
 * This header file exposes internal functions that are only made visible for
 * unit testing. Other clients are forbidden.
 */

typedef struct _nrminttable_t {
  int number;          /* Number of metrics in the table */
  int allocated;       /* Current number of metrics allocated */
  int max_size;        /* Maximum number of non-forced metrics */
  nrmetric_t* metrics; /* The metrics themselves */
  nrpool_t* strpool;   /* String pool containing the metric names */
} nrminttable_t;

/*
 * Apdex metrics do not have the COUNT, TOTAL, or EXCLUSIVE data attributes,
 * and instead have SATISFYING, TOLERATING, and FAILING.  We reduce the size
 * of the metric structure by reusing fields.
 *
 * Members of this enumeration is used as an index into an array.
 */
typedef enum _nr_metric_data_attribute_t {
  NRM_COUNT = 0,
  NRM_SATISFYING = 0,
  NRM_TOTAL = 1,
  NRM_TOLERATING = 1,
  NRM_EXCLUSIVE = 2,
  NRM_FAILING = 2,
  NRM_MIN = 3,
  NRM_MAX = 4,
  NRM_SUMSQUARES = 5,
  NRM_MUST_BE_GREATEST = 6
} nr_metric_data_attribute_t;

typedef struct _nrmintmetric_t {
  uint32_t hash;  /* Metric hash identifier for quick compares */
  int left;       /* Index of binary tree left child. -1 means empty */
  int right;      /* Index of binary tree right child. -1 means empty */
  uint32_t flags; /* Additional metric information */
  int name_index; /* String pool index of metric name */
  nrtime_t mdata[NRM_MUST_BE_GREATEST]; /* The actual metric data */
} nrmintmetric_t;

extern nrmetric_t* nrm_find_internal(nrmtable_t* table,
                                     const char* name,
                                     uint32_t hash);
extern nrmetric_t* nrm_create(nrmtable_t* table,
                              const char* name,
                              uint32_t hash);
extern nr_status_t nrm_table_validate(const nrmtable_t* table);

#endif
