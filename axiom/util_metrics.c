/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "util_buffer.h"
#include "util_hash.h"
#include "util_json.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_metrics_private.h"
#include "util_number_converter.h"
#include "util_object.h"
#include "util_string_pool.h"
#include "util_strings.h"
#include "util_time.h"

#define NRM_CREATE_ACCESSOR_FUNCTION(FN_NAME, ATTRIBUTE) \
  nrtime_t FN_NAME(const nrmetric_t* metric) {           \
    if (0 == metric) {                                   \
      return 0;                                          \
    }                                                    \
    return metric->mdata[ATTRIBUTE];                     \
  }

NRM_CREATE_ACCESSOR_FUNCTION(nrm_satisfying, NRM_SATISFYING)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_tolerating, NRM_TOLERATING)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_failing, NRM_FAILING)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_count, NRM_COUNT)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_total, NRM_TOTAL)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_exclusive, NRM_EXCLUSIVE)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_min, NRM_MIN)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_max, NRM_MAX)
NRM_CREATE_ACCESSOR_FUNCTION(nrm_sumsquares, NRM_SUMSQUARES)

#define NRM_DEFAULT_MAX_SIZE 2048

nrmtable_t* nrm_table_create(int max_size) {
  nrmtable_t* table;

  if (max_size <= 0) {
    max_size = NRM_DEFAULT_MAX_SIZE;
  }

  table = (nrmtable_t*)nr_zalloc(sizeof(nrmtable_t));

  table->number = 0;
  table->allocated = max_size;
  table->metrics = (nrmetric_t*)nr_calloc(table->allocated, sizeof(nrmetric_t));
  table->strpool = nr_string_pool_create();
  table->max_size = max_size;

  return table;
}

void nrm_table_destroy(nrmtable_t** table_p) {
  nrmtable_t* table;

  if ((0 == table_p) || (0 == *table_p)) {
    return;
  }

  table = *table_p;
  nr_free(table->metrics);
  nr_string_pool_destroy(&table->strpool);
  table->number = 0;
  nr_realfree((void**)table_p);
}

int nrm_is_apdex(const nrmetric_t* metric) {
  if (metric) {
    return (metric->flags & MET_IS_APDEX) ? 1 : 0;
  }
  return 0;
}

int nrm_is_forced(const nrmetric_t* metric) {
  if (metric) {
    return (MET_FORCED & metric->flags) ? 1 : 0;
  }
  return 0;
}

static uint32_t nrm_hash(const char* name) {
  return nr_mkhash(name, 0);
}

nrmetric_t* nrm_find_internal(nrmtable_t* table,
                              const char* name,
                              uint32_t hash) {
  int i;

  if ((0 == table) || (0 == table->number) || (0 == table->metrics)) {
    return 0;
  }

  i = 0;
  while (-1 != i) {
    nrmetric_t* metric = &table->metrics[i];

    if (hash == metric->hash) {
      const char* metric_name
          = nr_string_get(table->strpool, metric->name_index);

      if (0 == nr_strcmp(name, metric_name)) {
        return metric;
      }
    }

    if (metric->hash < hash) {
      i = metric->left;
    } else {
      i = metric->right;
    }
  }

  return 0;
}

nrmetric_t* nrm_find(nrmtable_t* table, const char* name) {
  uint32_t hash = nrm_hash(name);

  return nrm_find_internal(table, name, hash);
}

/*
 * Note : This function assumes that the metric to be added does not
 *        exist within the table already.  The caller should therefore
 *        first use nrm_find.
 */
nrmetric_t* nrm_create(nrmtable_t* table, const char* name, uint32_t hash) {
  int i;
  nrmetric_t* new_metric;
  int new_metric_index;

  if ((0 == table) || (0 == name)) {
    return 0;
  }

  if (table->number >= table->allocated) {
    table->allocated += NRM_DEFAULT_MAX_SIZE;
    table->metrics = (nrmetric_t*)nr_realloc(
        table->metrics, table->allocated * sizeof(nrmetric_t));
  }

  new_metric_index = table->number;
  table->number += 1;
  new_metric = &table->metrics[new_metric_index];

  nr_memset((void*)new_metric, 0, sizeof(*new_metric));

  new_metric->hash = hash;
  new_metric->left = -1;
  new_metric->right = -1;
  new_metric->flags = 0;
  new_metric->name_index = nr_string_add(table->strpool, name);
  new_metric->mdata[NRM_MIN] = NR_TIME_MAX;

  if (0 == new_metric_index) {
    return new_metric;
  }

  i = 0;
  for (;;) {
    nrmetric_t* test_metric = &table->metrics[i];

    if (test_metric->hash < hash) {
      i = test_metric->left;
      if (-1 == i) {
        test_metric->left = new_metric_index;
        return new_metric;
      }
    } else {
      i = test_metric->right;
      if (-1 == i) {
        test_metric->right = new_metric_index;
        return new_metric;
      }
    }
  }
}

const nrmetric_t* nrm_get_metric(const nrmtable_t* table, int i) {
  if (table && (i >= 0) && (i < table->number)) {
    return table->metrics + i;
  }
  return 0;
}

static int nrm_is_full(const nrmtable_t* table) {
  if (table && (table->number >= table->max_size)) {
    return 1;
  }
  return 0;
}

static nrmetric_t* nrm_find_or_create(int force,
                                      nrmtable_t* table,
                                      const char* name) {
  nrmetric_t* metric;
  uint32_t hash = nrm_hash(name);

  if ((0 == table) || (0 == name)) {
    return 0;
  }

  metric = nrm_find_internal(table, name, hash);
  if (0 == metric) {
    if ((1 == nrm_is_full(table)) && (0 == force)) {
      nrm_force_add(table, "Supportability/MetricsDropped", 0);
      return 0;
    }
    metric = nrm_create(table, name, hash);
  }

  if (force && metric) {
    metric->flags |= MET_FORCED;
  }
  return metric;
}

void nrm_add_internal(int force,
                      nrmtable_t* table,
                      const char* name,
                      nrtime_t count,
                      nrtime_t total,
                      nrtime_t exclusive,
                      nrtime_t min,
                      nrtime_t max,
                      nrtime_t sum_of_squares) {
  nrmetric_t* metric = nrm_find_or_create(force, table, name);

  if (0 == metric) {
    return;
  }

  metric->mdata[NRM_COUNT] += count;
  metric->mdata[NRM_TOTAL] += total;
  metric->mdata[NRM_EXCLUSIVE] += exclusive;

  if (min < metric->mdata[NRM_MIN]) {
    metric->mdata[NRM_MIN] = min;
  }

  if (max > metric->mdata[NRM_MAX]) {
    metric->mdata[NRM_MAX] = max;
  }

  metric->mdata[NRM_SUMSQUARES] += sum_of_squares;
}

void nrm_add_ex(nrmtable_t* table,
                const char* name,
                nrtime_t duration,
                nrtime_t exclusive) {
  nrm_add_internal(0, table, name, 1, duration, exclusive, duration, duration,
                   duration * duration);
}

void nrm_force_add_ex(nrmtable_t* table,
                      const char* name,
                      nrtime_t duration,
                      nrtime_t exclusive) {
  nrm_add_internal(1, table, name, 1, duration, exclusive, duration, duration,
                   duration * duration);
}

void nrm_add(nrmtable_t* table, const char* name, nrtime_t duration) {
  nrm_add_internal(0, table, name, 1, duration, duration, duration, duration,
                   duration * duration);
}

void nrm_force_add(nrmtable_t* table, const char* name, nrtime_t duration) {
  nrm_add_internal(1, table, name, 1, duration, duration, duration, duration,
                   duration * duration);
}

void nrm_add_apdex_internal(int force,
                            nrmtable_t* table,
                            const char* name,
                            nrtime_t satisfying,
                            nrtime_t tolerating,
                            nrtime_t failing,
                            nrtime_t min_apdex,
                            nrtime_t max_apdex) {
  nrmetric_t* metric = nrm_find_or_create(force, table, name);

  if (0 == metric) {
    return;
  }

  metric->flags |= MET_IS_APDEX;

  metric->mdata[NRM_SATISFYING] += satisfying;
  metric->mdata[NRM_TOLERATING] += tolerating;
  metric->mdata[NRM_FAILING] += failing;

  if (min_apdex < metric->mdata[NRM_MIN]) {
    metric->mdata[NRM_MIN] = min_apdex;
  }

  if (max_apdex > metric->mdata[NRM_MAX]) {
    metric->mdata[NRM_MAX] = max_apdex;
  }
}

void nrm_add_apdex(nrmtable_t* table,
                   const char* name,
                   nrtime_t satisfying,
                   nrtime_t tolerating,
                   nrtime_t failing,
                   nrtime_t apdex) {
  nrm_add_apdex_internal(0, table, name, satisfying, tolerating, failing, apdex,
                         apdex);
}

void nrm_force_add_apdex(nrmtable_t* table,
                         const char* name,
                         nrtime_t satisfying,
                         nrtime_t tolerating,
                         nrtime_t failing,
                         nrtime_t apdex) {
  nrm_add_apdex_internal(1, table, name, satisfying, tolerating, failing, apdex,
                         apdex);
}

int nrm_table_size(const nrmtable_t* tp) {
  if (0 == tp) {
    return 0;
  }

  return tp->number;
}

const char* nrm_get_name(const nrmtable_t* table, const nrmetric_t* met) {
  if (nrunlikely((0 == table) || (0 == met))) {
    return 0;
  }

  return nr_string_get(table->strpool, met->name_index);
}

static void nr_metric_data_as_json_to_buffer(nrbuf_t* buf,
                                             const nrmetric_t* met) {
  char tmp[512];
  int sl;

  if (NULL == met) {
    return;
  }

  if (met->flags & MET_IS_APDEX) {
    nrtime_t satisfying = met->mdata[NRM_SATISFYING];
    nrtime_t tolerating = met->mdata[NRM_TOLERATING];
    nrtime_t failing = met->mdata[NRM_FAILING];
    double min_apdex_seconds = (double)met->mdata[NRM_MIN] / NR_TIME_DIVISOR_D;
    double max_apdex_seconds = (double)met->mdata[NRM_MAX] / NR_TIME_DIVISOR_D;
    char buf_min_apdex_seconds[64];
    char buf_max_apdex_seconds[64];

    /*
     * Apdex metrics do not have a sum-of-squares data field.  In its place a
     * '0' is put so that apdex metrics will have 6 fields like normal metrics
     * and can be handled in the same manner by the collector.
     */
    nr_double_to_str(buf_min_apdex_seconds, sizeof(buf_min_apdex_seconds),
                     min_apdex_seconds);
    nr_double_to_str(buf_max_apdex_seconds, sizeof(buf_max_apdex_seconds),
                     max_apdex_seconds);
    sl = snprintf(tmp, sizeof(tmp),
                  "[" NR_TIME_FMT "," NR_TIME_FMT "," NR_TIME_FMT ",%s,%s,0]",
                  satisfying, tolerating, failing, buf_min_apdex_seconds,
                  buf_max_apdex_seconds);
  } else {
    nrtime_t count = met->mdata[NRM_COUNT];
    double total_seconds = (double)met->mdata[NRM_TOTAL] / NR_TIME_DIVISOR_D;
    double exclusive_seconds
        = (double)met->mdata[NRM_EXCLUSIVE] / NR_TIME_DIVISOR_D;
    double min_seconds = (double)met->mdata[NRM_MIN] / NR_TIME_DIVISOR_D;
    double max_seconds = (double)met->mdata[NRM_MAX] / NR_TIME_DIVISOR_D;
    double sum_of_squares_seconds
        = (double)met->mdata[NRM_SUMSQUARES] / NR_TIME_DIVISOR_D_SQUARE;
    char buf_total_seconds[BUFSIZ];
    char buf_exclusive_seconds[BUFSIZ];
    char buf_min_seconds[BUFSIZ];
    char buf_max_seconds[BUFSIZ];
    char buf_sum_of_squares_seconds[BUFSIZ];

    nr_double_to_str(buf_total_seconds, sizeof(buf_total_seconds),
                     total_seconds);
    nr_double_to_str(buf_exclusive_seconds, sizeof(buf_exclusive_seconds),
                     exclusive_seconds);
    nr_double_to_str(buf_min_seconds, sizeof(buf_min_seconds), min_seconds);
    nr_double_to_str(buf_max_seconds, sizeof(buf_max_seconds), max_seconds);
    nr_double_to_str(buf_sum_of_squares_seconds,
                     sizeof(buf_sum_of_squares_seconds),
                     sum_of_squares_seconds);
    sl = snprintf(tmp, sizeof(tmp), "[" NR_TIME_FMT ",%s,%s,%s,%s,%s]", count,
                  buf_total_seconds, buf_exclusive_seconds, buf_min_seconds,
                  buf_max_seconds, buf_sum_of_squares_seconds);
  }

  nr_buffer_add(buf, tmp, sl);
}

nr_status_t nrm_table_validate(const nrmtable_t* table) {
  int i;
  int used;

  if (0 == table) {
    return NR_FAILURE;
  }
  if (table->number < 0) {
    return NR_FAILURE;
  }
  if (table->allocated < 0) {
    return NR_FAILURE;
  }
  if (table->max_size < 0) {
    return NR_FAILURE;
  }
  if (table->number > table->allocated) {
    return NR_FAILURE;
  }

  used = table->number;

  if (used) {
    if (0 == table->metrics) {
      return NR_FAILURE;
    }
    if (0 == table->strpool) {
      return NR_FAILURE;
    }

    for (i = 0; i < used; i++) {
      const nrmetric_t* metric = &table->metrics[i];
      const char* name_string
          = nr_string_get(table->strpool, metric->name_index);

      if (metric->left < -1) {
        return NR_FAILURE;
      }
      if (metric->right < -1) {
        return NR_FAILURE;
      }
      if (metric->left >= used) {
        return NR_FAILURE;
      }
      if (metric->right >= used) {
        return NR_FAILURE;
      }
      if ((-1 != metric->left) && (metric->left <= i)) {
        return NR_FAILURE;
      }
      if ((-1 != metric->right) && (metric->right <= i)) {
        return NR_FAILURE;
      }
      if (0 == name_string) {
        return NR_FAILURE;
      }
    }
  }

  return NR_SUCCESS;
}

void nrm_duplicate_metric(nrmtable_t* table,
                          const char* current_name,
                          const char* new_name) {
  int force;
  const nrmetric_t* metric;

  if (NULL == table) {
    return;
  }
  if (NULL == current_name) {
    return;
  }
  if (NULL == new_name) {
    return;
  }

  metric = nrm_find(table, current_name);
  if (NULL == metric) {
    return;
  }

  force = nrm_is_forced(metric);

  nrm_add_internal(force, table, new_name, nrm_count(metric), nrm_total(metric),
                   nrm_exclusive(metric), nrm_min(metric), nrm_max(metric),
                   nrm_sumsquares(metric));
}

static void nr_metric_to_daemon_json_buffer(nrbuf_t* buf,
                                            const nrmetric_t* metric,
                                            const nrmtable_t* table) {
  nr_buffer_add(buf, NR_PSTR("{"));
  nr_buffer_add(buf, NR_PSTR("\"name\":"));
  nr_buffer_add_escape_json(buf, nrm_get_name(table, metric));

  nr_buffer_add(buf, NR_PSTR(",\"data\":"));
  nr_metric_data_as_json_to_buffer(buf, metric);

  if (nrm_is_forced(metric)) {
    /*
     * By default, metrics are assumed to be un-forced, so we only provide
     * this field if it is true.
     */
    nr_buffer_add(buf, NR_PSTR(",\"forced\":true"));
  }
  nr_buffer_add(buf, NR_PSTR("}"));
}

char* nr_metric_table_to_daemon_json(const nrmtable_t* table) {
  int i;
  nrbuf_t* buf;
  char* json;

  if (NULL == table) {
    return NULL;
  }

  buf = nr_buffer_create(8192, 8192);

  nr_buffer_add(buf, "[", 1);

  for (i = 0; i < table->number; i++) {
    if (i > 0) {
      nr_buffer_add(buf, NR_PSTR(","));
    }
    nr_metric_to_daemon_json_buffer(buf, table->metrics + i, table);
  }

  nr_buffer_add(buf, "]", 1);
  nr_buffer_add(buf, "\0", 1);

  json = nr_strdup((const char*)nr_buffer_cptr(buf));
  nr_buffer_destroy(&buf);

  return json;
}
