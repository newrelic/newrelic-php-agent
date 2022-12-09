/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains support for segment tests.
 */
#ifndef TEST_SEGMENT_HELPERS_HDR
#define TEST_SEGMENT_HELPERS_HDR

#include "nr_segment.h"
#include "nr_segment_datastore.h"
#include "nr_segment_external.h"
#include "tlib_main.h"
#include "util_metrics_private.h"
#include "nr_limits.h"
#include "nr_txn.h"
#include "nr_txn_private.h"

#define test_txn_untouched(X1, X2) \
  test_txn_untouched_fn((X1), (X2), __FILE__, __LINE__)
#define test_metric_created(TESTNAME, METRICS, FLAGS, DURATION, NAME) \
  do {                                                                \
    nrtime_t test_metric_duration = (DURATION);                       \
                                                                      \
    test_segment_helper_metric_created_fn(                            \
        (TESTNAME), (METRICS), (FLAGS), test_metric_duration,         \
        test_metric_duration, (NAME), __FILE__, __LINE__);            \
  } while (0)
#define test_metric_created_ex(TESTNAME, METRICS, FLAGS, DURATION, EXC, NAME) \
  test_segment_helper_metric_created_fn((TESTNAME), (METRICS), (FLAGS),       \
                                        (DURATION), (EXC), (NAME), __FILE__,  \
                                        __LINE__)
#define test_metric_table_size(...) \
  test_metric_table_size_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_metric_vector_size(VEC, EXPECTED_SIZE)                  \
  do {                                                               \
    size_t test_metric_vector_size_actual = nr_vector_size(VEC);     \
    tlib_pass_if_size_t_equal("metric vector size", (EXPECTED_SIZE), \
                              test_metric_vector_size_actual);       \
  } while (0)
#define test_segment_metric_created(...) \
  test_segment_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_txn_metric_created(...) \
  test_txn_metric_created_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_txn_metric_is(...) \
  test_txn_metric_is_fn(__VA_ARGS__, __FILE__, __LINE__)
#define test_metric_values_are(...) \
  test_metric_values_are_fn(__VA_ARGS__, __FILE__, __LINE__)

/*
 * These functions are all marked as unused, as they likely won't all be
 * exercised by a single set of unit tests.
 */

static NRUNUSED void test_metric_values_are_fn(const char* testname,
                                               const nrmetric_t* actual,
                                               uint32_t flags,
                                               nrtime_t count,
                                               nrtime_t total,
                                               nrtime_t exclusive,
                                               nrtime_t min,
                                               nrtime_t max,
                                               nrtime_t sumsquares,
                                               const char* file,
                                               int line) {
  test_pass_if_true_file_line(testname, 0 != actual, file, line, "actual=%p",
                              actual);

  if (actual) {
    test_pass_if_true_file_line(testname, flags == actual->flags, file, line,
                                "flags=%u actual->flags=%u", flags,
                                actual->flags);
    test_pass_if_true_file_line(
        testname, nrm_count(actual) == count, file, line,
        "nrm_count (actual)=" NR_TIME_FMT " count=" NR_TIME_FMT,
        nrm_count(actual), count);
    test_pass_if_true_file_line(
        testname, nrm_total(actual) == total, file, line,
        "nrm_total (actual)=" NR_TIME_FMT " total=" NR_TIME_FMT,
        nrm_total(actual), total);
    test_pass_if_true_file_line(
        testname, nrm_exclusive(actual) == exclusive, file, line,
        "nrm_exclusive (actual)=" NR_TIME_FMT " exclusive=" NR_TIME_FMT,
        nrm_exclusive(actual), exclusive);
    test_pass_if_true_file_line(testname, nrm_min(actual) == min, file, line,
                                "nrm_min (actual)=" NR_TIME_FMT
                                " min=" NR_TIME_FMT,
                                nrm_min(actual), min);
    test_pass_if_true_file_line(testname, nrm_max(actual) == max, file, line,
                                "nrm_max (actual)=" NR_TIME_FMT
                                " max=" NR_TIME_FMT,
                                nrm_max(actual), max);
    test_pass_if_true_file_line(
        testname, nrm_sumsquares(actual) == sumsquares, file, line,
        "nrm_sumsquares (actual)=" NR_TIME_FMT " sumsquares=" NR_TIME_FMT,
        nrm_sumsquares(actual), sumsquares);
  }
}

static NRUNUSED void test_txn_untouched_fn(const char* testname,
                                           const nrtxn_t* txn,
                                           const char* file,
                                           int line) {
  test_pass_if_true_file_line(testname,
                              0 == nrm_table_size(txn->scoped_metrics), file,
                              line, "nrm_table_size (txn->scoped_metrics)=%d",
                              nrm_table_size(txn->scoped_metrics));
  test_pass_if_true_file_line(testname,
                              0 == nrm_table_size(txn->unscoped_metrics), file,
                              line, "nrm_table_size (txn->unscoped_metrics)=%d",
                              nrm_table_size(txn->unscoped_metrics));

  // An empty transaction will have a root segment.
  tlib_pass_if_not_null(testname, txn->segment_root);
  tlib_pass_if_size_t_equal(testname, 0, txn->segment_count);
}

static NRUNUSED void test_segment_metric_created_fn(const char* testname,
                                                    nr_vector_t* metrics,
                                                    const char* metric_name,
                                                    bool scoped,
                                                    const char* file,
                                                    int line) {
  bool passed = false;
  for (size_t i = 0; i < nr_vector_size(metrics); i++) {
    nr_segment_metric_t* sm = nr_vector_get(metrics, i);
    if (0 == nr_strcmp(metric_name, sm->name) && (scoped == sm->scoped)) {
      passed = true;
      break;
    }
  }

  test_pass_if_true_file_line(testname, passed, file, line,
                              "metric %s (scoped %d) not created", metric_name,
                              scoped);
}

static NRUNUSED void test_txn_metric_created_fn(const char* testname,
                                                nrmtable_t* metrics,
                                                const char* expected,
                                                const char* file,
                                                int line) {
  test_pass_if_true_file_line(testname, NULL != nrm_find(metrics, expected),
                              file, line, "expected=%s", expected);
}

static NRUNUSED void test_metric_table_size_fn(const char* testname,
                                               const nrmtable_t* metrics,
                                               int expected_size,
                                               const char* file,
                                               int line) {
  int actual_size = nrm_table_size(metrics);

  test_pass_if_true_file_line(testname, expected_size == actual_size, file,
                              line, "expected_size=%d actual_size=%d",
                              expected_size, actual_size);
}

static NRUNUSED void test_segment_helper_metric_created_fn(const char* testname,
                                                           nrmtable_t* metrics,
                                                           uint32_t flags,
                                                           nrtime_t duration,
                                                           nrtime_t exclusive,
                                                           const char* name,
                                                           const char* file,
                                                           int line) {
  const nrmetric_t* m = nrm_find(metrics, name);
  const char* nm = nrm_get_name(metrics, m);

  test_pass_if_true_file_line(testname, 0 != m, file, line, "m=%p", m);
  test_pass_if_true_file_line(testname, 0 == nr_strcmp(nm, name), file, line,
                              "nm=%s name=%s", NRSAFESTR(nm), NRSAFESTR(name));

  if (0 != m) {
    test_pass_if_true_file_line(testname, flags == m->flags, file, line,
                                "name=%s flags=%u m->flags=%u", name, flags,
                                m->flags);
    test_pass_if_true_file_line(testname, nrm_count(m) == 1, file, line,
                                "name=%s nrm_count (m)=" NR_TIME_FMT, name,
                                nrm_count(m));
    test_pass_if_true_file_line(testname, nrm_total(m) == duration, file, line,
                                "name=%s nrm_total (m)=" NR_TIME_FMT
                                " duration=" NR_TIME_FMT,
                                name, nrm_total(m), duration);
    test_pass_if_true_file_line(
        testname, nrm_exclusive(m) == exclusive, file, line,
        "name=%s nrm_exclusive (m)=" NR_TIME_FMT " exclusive=" NR_TIME_FMT,
        name, nrm_exclusive(m), exclusive);
    test_pass_if_true_file_line(testname, nrm_min(m) == duration, file, line,
                                "name=%s nrm_min (m)=" NR_TIME_FMT
                                " duration=" NR_TIME_FMT,
                                name, nrm_min(m), duration);
    test_pass_if_true_file_line(testname, nrm_max(m) == duration, file, line,
                                "name=%s nrm_max (m)=" NR_TIME_FMT
                                " duration=" NR_TIME_FMT,
                                name, nrm_max(m), duration);
    test_pass_if_true_file_line(
        testname, nrm_sumsquares(m) == (duration * duration), file, line,
        "name=%s nrm_sumsquares (m)=" NR_TIME_FMT " duration=" NR_TIME_FMT,
        name, nrm_sumsquares(m), duration);
  }
}

static NRUNUSED void test_txn_metric_is_fn(const char* testname,
                                           nrmtable_t* table,
                                           uint32_t flags,
                                           const char* name,
                                           nrtime_t count,
                                           nrtime_t total,
                                           nrtime_t exclusive,
                                           nrtime_t min,
                                           nrtime_t max,
                                           nrtime_t sumsquares,
                                           const char* file,
                                           int line) {
  const nrmetric_t* m = nrm_find(table, name);
  const char* nm = nrm_get_name(table, m);

  test_pass_if_true_file_line(testname, 0 != m, file, line, "m=%p", m);
  test_pass_if_true_file_line(testname, 0 == nr_strcmp(nm, name), file, line,
                              "nm=%s name=%s", nm, name);

  test_metric_values_are_fn(testname, m, flags, count, total, exclusive, min,
                            max, sumsquares, file, line);
}

static NRUNUSED nrtxn_t* new_txn(int background) {
  nrapp_t app;
  nrtxn_t* txn;

  nr_memset(&app, 0, sizeof(app));

  app.info.high_security = 0;
  app.state = NR_APP_OK;
  app.connect_reply = nro_create_from_json(
      "{\"collect_traces\":true,\"collect_errors\":true}");
  app.info.license = nr_strdup("0123456789012345678901234567890123456789");
  app.rnd = NULL;
  app.limits = (nr_app_limits_t){
      .analytics_events = NR_MAX_ANALYTIC_EVENTS,
      .custom_events = NR_DEFAULT_CUSTOM_EVENTS_MAX_SAMPLES_STORED,
      .error_events = NR_MAX_ERRORS,
      .span_events = NR_DEFAULT_SPAN_EVENTS_MAX_SAMPLES_STORED,
  };

  txn = nr_txn_begin(&app, &nr_txn_test_options, 0);
  if (0 == txn) {
    return txn;
  }

  nr_free(app.info.license);
  nro_delete(app.connect_reply);

  if (background) {
    nr_txn_set_as_background_job(txn, 0);
  }

  /*
   * Clear the metric tables to easily test if new metrics have been created.
   */
  nrm_table_destroy(&txn->unscoped_metrics);
  txn->unscoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  nrm_table_destroy(&txn->scoped_metrics);
  txn->scoped_metrics = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  return txn;
}

/*
 * Purpose : Ends a segment without nulling out the segment pointer.
 *
 * WARNING : This can only be used safely when the segment priority queue is
 *           disabled.
 */
static NRUNUSED bool test_segment_end_and_keep(nr_segment_t** segment_ptr) {
  nr_segment_t* segment;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  return nr_segment_end(&segment);
}

/*
 * Purpose : Ends an external  segment without nulling out the segment pointer.
 *
 * WARNING : This can only be used safely when the segment priority queue is
 *           disabled.
 */
static NRUNUSED bool test_segment_external_end_and_keep(
    nr_segment_t** segment_ptr,
    nr_segment_external_params_t* params) {
  nr_segment_t* segment;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  return nr_segment_external_end(&segment, params);
}

/*
 * Purpose : Ends a datastore segment without nulling out the segment pointer.
 *
 * WARNING : This can only be used safely when the segment priority queue is
 *           disabled.
 */
static NRUNUSED bool test_segment_datastore_end_and_keep(
    nr_segment_t** segment_ptr,
    nr_segment_datastore_params_t* params) {
  nr_segment_t* segment;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  return nr_segment_datastore_end(&segment, params);
}

#endif /* TEST_SEGMENT_HELPERS_HDR */
