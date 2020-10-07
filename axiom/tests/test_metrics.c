/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "util_memory.h"
#include "util_metrics.h"
#include "util_metrics_private.h"
#include "util_reply.h"
#include "util_strings.h"

#include "tlib_main.h"

#define test_pass_if_true(M, T, ...) \
  tlib_pass_if_true_f((M), (T), file, line, #T, __VA_ARGS__)
#define test_metric_json(...) \
  test_metric_json_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_metric_json_fn(const char* testname,
                                const nrmtable_t* table,
                                const char* expected_json,
                                const char* file,
                                int line) {
  char* json;

  json = nr_metric_table_to_daemon_json(table);
  test_pass_if_true(testname, 0 == nr_strcmp(expected_json, json),
                    "json=%s\nexpected_json=%s", NRSAFESTR(json),
                    NRSAFESTR(expected_json));

  /* Ensure that the JSON is valid. */
  if (json) {
    nrobj_t* obj = nro_create_from_json(json);

    test_pass_if_true(testname, 0 != obj, "json=%s", NRSAFESTR(json));
    nro_delete(obj);
    nr_free(json);
  }
}

static void test_find_internal_bad_parameters(void) {
  nrmtable_t* table = nrm_table_create(0);
  nrmetric_t* metric;

  metric = nrm_find_internal(0, "name", 12345);
  tlib_pass_if_true("missing table", 0 == metric, "metric=%p", metric);

  metric = nrm_find_internal(table, "name", 12345);
  tlib_pass_if_true("empty table", 0 == metric, "metric=%p", metric);

  metric = nrm_find_internal(table, 0, 12345);
  tlib_pass_if_true("null name", 0 == metric, "metric=%p", metric);

  nrm_table_destroy(&table);
}

static void test_accessor_bad_parameters(void) {
  nrtime_t data;

  data = nrm_satisfying(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_tolerating(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_failing(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_count(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_total(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_exclusive(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_min(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_max(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
  data = nrm_sumsquares(0);
  tlib_pass_if_true("null metric", 0 == data, "data=" NR_TIME_FMT, data);
}

static void test_find_create(void) {
  int i;
  nr_status_t rv;
  int limit;
  const char* name;
  nrmtable_t* table = nrm_table_create(0);
  nrmetric_t* metric;
  uint32_t hash = 12345;

  /*
   * Create and find.
   */
  metric = nrm_create(table, "name", hash);
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("metric created", 0 != metric, "metric=%p", metric);
  tlib_pass_if_true("metric created", 0 == nr_strcmp("name", name), "name=%s",
                    NRSAFESTR(name));

  metric = nrm_find_internal(table, "name", hash + 1);
  tlib_pass_if_true("different hash", 0 == metric, "metric=%p", metric);

  metric = nrm_find_internal(table, "DIFFERENT", hash);
  tlib_pass_if_true("different name", 0 == metric, "metric=%p", metric);

  metric = nrm_find_internal(table, "name", hash);
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("metric found", 0 != metric, "metric=%p", metric);
  tlib_pass_if_true("metric found", 0 == nr_strcmp("name", name), "name=%s",
                    NRSAFESTR(name));

  /*
   * Add and find some more metrics with the same hash to test hash collisions.
   */
  metric = nrm_create(table, "name", hash);
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("metric", 0 != metric, "metric=%p", metric);
  tlib_pass_if_true("metric", 0 == nr_strcmp("name", name), "name=%s",
                    NRSAFESTR(name));

  metric = nrm_create(table, "name2", hash);
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("metric with different name", 0 != metric, "metric=%p",
                    metric);
  tlib_pass_if_true("metric with different name", 0 == nr_strcmp("name2", name),
                    "name=%s", NRSAFESTR(name));

  metric = nrm_find_internal(table, "name", hash);
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("metric found", 0 != metric, "metric=%p", metric);
  tlib_pass_if_true("metric found", 0 == nr_strcmp("name", name), "name=%s",
                    NRSAFESTR(name));

  nrm_table_destroy(&table);

  /* Create lots of metrics */
  limit = 1024;
  table = nrm_table_create(limit + 1);
  for (i = 0; i < limit; i++) {
    char name_buf[256];

    snprintf(name_buf, sizeof(name_buf), "%dname%d", i, i);
    nrm_add_internal(0, table, name_buf, 1, 2, 3, 4, 5, 6);
  }

  /* Validate the table */
  rv = nrm_table_validate(table);
  tlib_pass_if_true("table is valid after lots of metrics inserted",
                    NR_SUCCESS == rv, "rv=%d", (int)rv);

  /* Find all of the metrics */
  for (i = 0; i < limit; i++) {
    char name_buf[256];

    snprintf(name_buf, sizeof(name_buf), "%dname%d", i, i);
    metric = nrm_find(table, name_buf);
    tlib_pass_if_true("metric found after lots of metrics inserted",
                      0 != metric, "metric=%p", metric);
  }

  nrm_table_destroy(&table);
}

#define test_metric_attribute(T, V1, V2) \
  test_metric_attribute_fn((T), #V1, (V1), #V2, (V2), __FILE__, __LINE__)

static void test_metric_attribute_fn(const char* testname,
                                     const char* expression1,
                                     nrtime_t value1,
                                     const char* expression2,
                                     nrtime_t value2,
                                     const char* file,
                                     int line) {
  test_pass_if_true(testname, value1 == value2,
                    "%s=" NR_TIME_FMT " %s=" NR_TIME_FMT, expression1, value1,
                    expression2, value2);
}

static void test_add(void) {
  const char* testname;
  nrmtable_t* table = nrm_table_create(0);
  nrmetric_t* metric;
  const char* name;

  nrm_add(table, "metric_name", 10 * NR_TIME_DIVISOR);

  testname = "single nrm_add";
  metric = nrm_find(table, "metric_name");
  test_metric_json(testname, table,
                   "[{\"name\":\"metric_name\",\"data\":[1,10.00000,10.00000,"
                   "10.00000,10.00000,100.00000]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("metric_name", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_count(metric), 1);
  test_metric_attribute(testname, nrm_total(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_exclusive(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_min(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_sumsquares(metric),
                        100 * (NR_TIME_DIVISOR * NR_TIME_DIVISOR));

  nrm_add(table, "metric_name", 9 * NR_TIME_DIVISOR);  /* Min */
  nrm_add(table, "metric_name", 11 * NR_TIME_DIVISOR); /* Max */

  testname = "multiple nrm_add";
  metric = nrm_find(table, "metric_name");
  test_metric_json(testname, table,
                   "[{\"name\":\"metric_name\",\"data\":[3,30.00000,30.00000,9."
                   "00000,11.00000,302.00000]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("metric_name", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_count(metric), 3);
  test_metric_attribute(testname, nrm_total(metric),
                        (10 + 9 + 11) * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_exclusive(metric),
                        (10 + 9 + 11) * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_min(metric), 9 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 11 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_sumsquares(metric),
                        (100 + 81 + 121) * (NR_TIME_DIVISOR * NR_TIME_DIVISOR));

  nrm_table_destroy(&table);
}

static void test_add_ex(void) {
  const char* testname;
  nrmtable_t* table = nrm_table_create(0);
  nrmetric_t* metric;
  const char* name;

  nrm_add_ex(table, "metric_name", 10 * NR_TIME_DIVISOR, 5 * NR_TIME_DIVISOR);

  testname = "single nrm_add_ex";
  metric = nrm_find(table, "metric_name");
  test_metric_json(testname, table,
                   "[{\"name\":\"metric_name\",\"data\":[1,10.00000,5.00000,10."
                   "00000,10.00000,100.00000]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("metric_name", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_count(metric), 1);
  test_metric_attribute(testname, nrm_total(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_exclusive(metric), 5 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_min(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 10 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_sumsquares(metric),
                        100 * (NR_TIME_DIVISOR * NR_TIME_DIVISOR));

  nrm_add_ex(table, "metric_name", 9 * NR_TIME_DIVISOR,
             4 * NR_TIME_DIVISOR); /* Min */
  nrm_add_ex(table, "metric_name", 11 * NR_TIME_DIVISOR,
             3 * NR_TIME_DIVISOR); /* Max */

  testname = "multiple nrm_add_ex";
  metric = nrm_find(table, "metric_name");
  test_metric_json(testname, table,
                   "[{\"name\":\"metric_name\",\"data\":[3,30.00000,12.00000,9."
                   "00000,11.00000,302.00000]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("metric_name", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_count(metric), 3);
  test_metric_attribute(testname, nrm_total(metric),
                        (10 + 9 + 11) * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_exclusive(metric),
                        (5 + 4 + 3) * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_min(metric), 9 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 11 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_sumsquares(metric),
                        (100 + 81 + 121) * (NR_TIME_DIVISOR * NR_TIME_DIVISOR));

  nrm_table_destroy(&table);
}

#define test_metrics_equal(...) \
  test_metrics_equal_fn(__VA_ARGS__, __FILE__, __LINE__)

static void test_metrics_equal_fn(const char* testname,
                                  nrmtable_t* table1,
                                  nrmtable_t* table2,
                                  const char* name,
                                  const char* file,
                                  int line) {
  const nrmetric_t* metric1 = nrm_find(table1, name);
  const nrmetric_t* metric2 = nrm_find(table2, name);

  test_pass_if_true(testname, 0 != metric1, "metric1=%p", metric1);
  test_pass_if_true(testname, 0 != metric2, "metric2=%p", metric2);

  if (metric1 && metric2) {
    test_metric_attribute_fn(testname, "nrm_count      (metric1)",
                             nrm_count(metric1), "nrm_count      (metric2)",
                             nrm_count(metric2), file, line);
    test_metric_attribute_fn(testname, "nrm_total      (metric1)",
                             nrm_total(metric1), "nrm_total      (metric2)",
                             nrm_total(metric2), file, line);
    test_metric_attribute_fn(testname, "nrm_exclusive  (metric1)",
                             nrm_exclusive(metric1), "nrm_exclusive  (metric2)",
                             nrm_exclusive(metric2), file, line);
    test_metric_attribute_fn(testname, "nrm_min        (metric1)",
                             nrm_min(metric1), "nrm_min        (metric2)",
                             nrm_min(metric2), file, line);
    test_metric_attribute_fn(testname, "nrm_max        (metric1)",
                             nrm_max(metric1), "nrm_max        (metric2)",
                             nrm_max(metric2), file, line);
    test_metric_attribute_fn(
        testname, "nrm_sumsquares (metric1)", nrm_sumsquares(metric1),
        "nrm_sumsquares (metric2)", nrm_sumsquares(metric2), file, line);
  }
}

static void test_force_add(void) {
  nrmtable_t* table;
  nrmtable_t* t1;
  nrmtable_t* t2;
  nrmetric_t* metric;

  /*
   * Test that the metric is indeed forced.
   */
  table = nrm_table_create(1);
  nrm_add(table, "fill_up_table", 0);
  nrm_add(table, "NOT_FORCED", 0);
  metric = nrm_find(table, "NOT_FORCED");
  tlib_pass_if_true("table full unforced metric", 0 == metric, "metric=%p",
                    metric);
  nrm_force_add(table, "FORCED", 0);
  metric = nrm_find(table, "FORCED");
  tlib_pass_if_true("table full forced metric", 0 != metric, "metric=%p",
                    metric);
  nrm_table_destroy(&table);

  /*
   * Test that nrm_add and nrm_force_add produce the same metrics.
   */
  t1 = nrm_table_create(10);
  t2 = nrm_table_create(10);
  nrm_add(t1, "metric_name", 10 * NR_TIME_DIVISOR);
  nrm_force_add(t2, "metric_name", 10 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add and nrm_force_add", t1, t2, "metric_name");
  nrm_add(t1, "metric_name", 9 * NR_TIME_DIVISOR);
  nrm_force_add(t2, "metric_name", 9 * NR_TIME_DIVISOR);
  nrm_add(t1, "metric_name", 11 * NR_TIME_DIVISOR);
  nrm_force_add(t2, "metric_name", 11 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add and nrm_force_add", t1, t2, "metric_name");
  nrm_table_destroy(&t1);
  nrm_table_destroy(&t2);
}

static void test_force_add_ex(void) {
  nrmtable_t* table;
  nrmetric_t* metric;
  nrmtable_t* t1;
  nrmtable_t* t2;

  /*
   * Test that the metric is indeed forced.
   */
  table = nrm_table_create(1);
  nrm_add_ex(table, "fill_up_table", 0, 0);
  nrm_add_ex(table, "NOT_FORCED", 0, 0);
  metric = nrm_find(table, "NOT_FORCED");
  tlib_pass_if_true("table full unforced metric ex", 0 == metric, "metric=%p",
                    metric);
  nrm_force_add_ex(table, "FORCED", 0, 0);
  metric = nrm_find(table, "FORCED");
  tlib_pass_if_true("table full forced metric ex", 0 != metric, "metric=%p",
                    metric);
  nrm_table_destroy(&table);

  /*
   * Test that nrm_add_ex and nrm_force_add_ex produce the same metrics.
   */
  t1 = nrm_table_create(10);
  t2 = nrm_table_create(10);
  nrm_add_ex(t1, "metric_name", 10 * NR_TIME_DIVISOR, 5 * NR_TIME_DIVISOR);
  nrm_force_add_ex(t2, "metric_name", 10 * NR_TIME_DIVISOR,
                   5 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add_ex and nrm_force_add_ex", t1, t2, "metric_name");
  nrm_add_ex(t1, "metric_name", 9 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_force_add_ex(t2, "metric_name", 9 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_add_ex(t1, "metric_name", 11 * NR_TIME_DIVISOR, 3 * NR_TIME_DIVISOR);
  nrm_force_add_ex(t2, "metric_name", 11 * NR_TIME_DIVISOR,
                   3 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add_ex and nrm_force_add_ex", t1, t2, "metric_name");
  nrm_table_destroy(&t1);
  nrm_table_destroy(&t2);
}

static void test_add_apdex(void) {
  const char* testname;
  nrmtable_t* table = nrm_table_create(0);
  nrmetric_t* metric;
  const char* name;

  nrm_add_apdex(table, "my_apdex", 11, 22, 33, 5 * NR_TIME_DIVISOR);

  testname = "single nrm_add_apdex";
  metric = nrm_find(table, "my_apdex");
  test_metric_json(
      testname, table,
      "[{\"name\":\"my_apdex\",\"data\":[11,22,33,5.00000,5.00000,0]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("my_apdex", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_satisfying(metric), 11);
  test_metric_attribute(testname, nrm_tolerating(metric), 22);
  test_metric_attribute(testname, nrm_failing(metric), 33);
  test_metric_attribute(testname, nrm_min(metric), 5 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 5 * NR_TIME_DIVISOR);

  nrm_add_apdex(table, "my_apdex", 10, 10, 10, 4 * NR_TIME_DIVISOR); /* Min */
  nrm_add_apdex(table, "my_apdex", 25, 35, 45, 6 * NR_TIME_DIVISOR); /* Max */

  testname = "multiple nrm_add";
  metric = nrm_find(table, "my_apdex");
  test_metric_json(
      testname, table,
      "[{\"name\":\"my_apdex\",\"data\":[46,67,88,4.00000,6.00000,0]}]");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true(testname, 0 == nr_strcmp("my_apdex", name), "name=%s",
                    NRSAFESTR(name));
  test_metric_attribute(testname, nrm_satisfying(metric), 11 + 10 + 25);
  test_metric_attribute(testname, nrm_tolerating(metric), 22 + 10 + 35);
  test_metric_attribute(testname, nrm_failing(metric), 33 + 10 + 45);
  test_metric_attribute(testname, nrm_min(metric), 4 * NR_TIME_DIVISOR);
  test_metric_attribute(testname, nrm_max(metric), 6 * NR_TIME_DIVISOR);

  nrm_table_destroy(&table);
}

static void test_force_add_apdex(void) {
  nrmtable_t* table;
  nrmetric_t* metric;
  nrmtable_t* t1;
  nrmtable_t* t2;

  /*
   * Test that the metric is indeed forced.
   */
  table = nrm_table_create(1);
  nrm_add_apdex(table, "fill_up_table", 0, 0, 0, 0);
  nrm_add_apdex(table, "NOT_FORCED", 0, 0, 0, 0);
  metric = nrm_find(table, "NOT_FORCED");
  tlib_pass_if_true("table full unforced apdex", 0 == metric, "metric=%p",
                    metric);
  nrm_force_add_apdex(table, "FORCED", 0, 0, 0, 0);
  metric = nrm_find(table, "FORCED");
  tlib_pass_if_true("table full forced apdex", 0 != metric, "metric=%p",
                    metric);
  nrm_table_destroy(&table);

  /*
   * Test that nrm_add_apdex and nrm_force_add_apdex produce the same metrics.
   */
  t1 = nrm_table_create(10);
  t2 = nrm_table_create(10);

  nrm_add_apdex(t1, "my_apdex", 11, 22, 33, 5 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(t2, "my_apdex", 11, 22, 33, 5 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add_apdex and nrm_force_add_apdex", t1, t2,
                     "my_apdex");
  nrm_add_apdex(t1, "my_apdex", 10, 10, 10, 4 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(t2, "my_apdex", 10, 10, 10, 4 * NR_TIME_DIVISOR);
  nrm_add_apdex(t1, "my_apdex", 25, 35, 45, 6 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(t2, "my_apdex", 25, 35, 45, 6 * NR_TIME_DIVISOR);
  test_metrics_equal("nrm_add_apdex and nrm_force_add_apdex", t1, t2,
                     "my_apdex");
  nrm_table_destroy(&t1);
  nrm_table_destroy(&t2);
}

static void test_add_internal(void) {
  nrmtable_t* table = nrm_table_create(1);
  nrmetric_t* metric;
  const char* name;

  nrm_add_internal(0,                                      /* forced */
                   table,                                  /* table */
                   "name1",                                /* name */
                   1,                                      /* count */
                   2 * NR_TIME_DIVISOR,                    /* total */
                   3 * NR_TIME_DIVISOR,                    /* exclusive */
                   4 * NR_TIME_DIVISOR,                    /* min */
                   5 * NR_TIME_DIVISOR,                    /* max */
                   6 * NR_TIME_DIVISOR * NR_TIME_DIVISOR); /* sum of squares */

  metric = nrm_find(table, "name1");
  name = nrm_get_name(table, metric);
  tlib_pass_if_true("nrm_add_internal", 0 == nr_strcmp("name1", name),
                    "name=%s", NRSAFESTR(name));
  test_metric_attribute("nrm_add_internal", nrm_count(metric), 1);
  test_metric_attribute("nrm_add_internal", nrm_total(metric),
                        2 * NR_TIME_DIVISOR);
  test_metric_attribute("nrm_add_internal", nrm_exclusive(metric),
                        3 * NR_TIME_DIVISOR);
  test_metric_attribute("nrm_add_internal", nrm_min(metric),
                        4 * NR_TIME_DIVISOR);
  test_metric_attribute("nrm_add_internal", nrm_max(metric),
                        5 * NR_TIME_DIVISOR);
  test_metric_attribute("nrm_add_internal", nrm_sumsquares(metric),
                        6 * NR_TIME_DIVISOR * NR_TIME_DIVISOR);

  /* Table full not forced */
  nrm_add_internal(0, table, "name2", 0, 0, 0, 0, 0, 0);
  /* Forced */
  nrm_add_internal(1, table, "name3", 0, 0, 0, 0, 0, 0);

  test_metric_json(
      "test_add_internal", table,
      "[{\"name\":\"name1\","
      "\"data\":[1,2.00000,3.00000,4.00000,5.00000,6.00000]},"
      "{\"name\":\"Supportability\\/"
      "MetricsDropped\",\"data\":[1,0.00000,0.00000,0.00000,0.00000,0.00000],"
      "\"forced\":true},"
      "{\"name\":\"name3\","
      "\"data\":[0,0.00000,0.00000,0.00000,0.00000,0.00000],\"forced\":true}]");

  nrm_table_destroy(&table);
}

static void test_add_apdex_internal(void) {
  nrmtable_t* table = nrm_table_create(1);
  nrmetric_t* metric;
  const char* name;

  nrm_add_apdex_internal(0,                    /* forced */
                         table,                /* table */
                         "name1",              /* name */
                         1,                    /* satisfying */
                         2,                    /* tolerating */
                         3,                    /* failing */
                         4 * NR_TIME_DIVISOR,  /* min apdex */
                         5 * NR_TIME_DIVISOR); /* max apdex */

  metric = nrm_find(table, "name1");
  name = nrm_get_name(table, metric);

  tlib_pass_if_true("nrm_add_apdex_internal", 0 == nr_strcmp("name1", name),
                    "name=%s", NRSAFESTR(name));
  test_metric_attribute("nrm_add_apdex_internal", nrm_satisfying(metric), 1);
  test_metric_attribute("nrm_add_apdex_internal", nrm_tolerating(metric), 2);
  test_metric_attribute("nrm_add_apdex_internal", nrm_failing(metric), 3);
  test_metric_attribute("nrm_add_apdex_internal", nrm_min(metric),
                        4 * NR_TIME_DIVISOR);
  test_metric_attribute("nrm_add_apdex_internal", nrm_max(metric),
                        5 * NR_TIME_DIVISOR);

  /* Table full not forced */
  nrm_add_apdex_internal(0, table, "name2", 0, 0, 0, 0, 0);
  /* Forced */
  nrm_add_apdex_internal(1, table, "name3", 0, 0, 0, 0, 0);

  test_metric_json("test_add_internal", table,
                   "[{\"name\":\"name1\","
                   "\"data\":[1,2,3,4.00000,5.00000,0]},"
                   "{\"name\":\"Supportability\\/"
                   "MetricsDropped\",\"data\":[1,0.00000,0.00000,0.00000,0."
                   "00000,0.00000],\"forced\":true},"
                   "{\"name\":\"name3\","
                   "\"data\":[0,0,0,0.00000,0.00000,0],\"forced\":true}]");

  nrm_table_destroy(&table);
}

static void test_add_bad_parameters(void) {
  nrmtable_t* table;

  /*
   * NULL table, don't blow up!
   */
  nrm_add_ex(0, "name", 5 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_force_add_ex(0, "name", 5 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_add(0, "name", 5 * NR_TIME_DIVISOR);
  nrm_force_add(0, "name", 5 * NR_TIME_DIVISOR);
  nrm_add_apdex(0, "name", 55, 44, 33, 2 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(0, "name", 55, 44, 33, 2 * NR_TIME_DIVISOR);

  /*
   * NULL name.
   */
  table = nrm_table_create(0);
  nrm_add_ex(table, 0, 5 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_force_add_ex(table, 0, 5 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_add(table, 0, 5 * NR_TIME_DIVISOR);
  nrm_force_add(table, 0, 5 * NR_TIME_DIVISOR);
  nrm_add_apdex(table, 0, 55, 44, 33, 2 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(table, 0, 55, 44, 33, 2 * NR_TIME_DIVISOR);
  tlib_pass_if_int_equal("NULL name metrics added", nrm_table_size(table), 0);
  nrm_table_destroy(&table);
}

static void test_duplicate_metric(void) {
  nrmtable_t* table;
  int table_size;

  table = nrm_table_create(0);
  nrm_force_add(table, "old", 123 * NR_TIME_DIVISOR);

  /*
   * NULL table, don't blow up!
   */
  nrm_duplicate_metric(NULL, NULL, NULL);
  nrm_duplicate_metric(NULL, "old", "new");

  /*
   * Bad parameters
   */
  nrm_duplicate_metric(table, NULL, "new");
  nrm_duplicate_metric(table, "old", NULL);
  nrm_duplicate_metric(table, "wrong_name", "new");
  table_size = nrm_table_size(table);
  tlib_pass_if_int_equal("bad parameters", table_size, 1);

  /*
   * Success
   */
  nrm_duplicate_metric(table, "old", "new");
  table_size = nrm_table_size(table);
  test_metric_json("duplicate success", table,
                   "[{\"name\":\"old\",\"data\":[1,123.00000,123.00000,123."
                   "00000,123.00000,15129.00000],\"forced\":true},"
                   "{\"name\":\"new\",\"data\":[1,123.00000,123.00000,123."
                   "00000,123.00000,15129.00000],\"forced\":true}]");

  nrm_table_destroy(&table);
}

static void test_metric_table_to_daemon_json(void) {
  nrmtable_t* table;
  char* json;

  tlib_pass_if_null("NULL table", nr_metric_table_to_daemon_json(NULL));

  table = nrm_table_create(10);

  json = nr_metric_table_to_daemon_json(table);
  tlib_pass_if_str_equal("empty table", json, "[]");
  nr_free(json);

  nrm_add(table, "nrm_add", 1 * NR_TIME_DIVISOR);
  nrm_force_add(table, "nrm_force_add", 2 * NR_TIME_DIVISOR);
  nrm_add_ex(table, "nrm_add_ex", 3 * NR_TIME_DIVISOR, 4 * NR_TIME_DIVISOR);
  nrm_force_add_ex(table, "nrm_force_add_ex", 5 * NR_TIME_DIVISOR,
                   6 * NR_TIME_DIVISOR);
  nrm_add_apdex(table, "nrm_add_apdex", 1, 2, 3, 44 * NR_TIME_DIVISOR);
  nrm_force_add_apdex(table, "nrm_force_add_apdex", 5, 6, 7,
                      88 * NR_TIME_DIVISOR);

  json = nr_metric_table_to_daemon_json(table);
  tlib_pass_if_str_equal(
      "empty table", json,
      "["
      "{\"name\":\"nrm_add\",\"data\":[1,1.00000,1.00000,1.00000,1.00000,1."
      "00000]},"
      "{\"name\":\"nrm_force_add\",\"data\":[1,2.00000,2.00000,2.00000,2.00000,"
      "4.00000],\"forced\":true},"
      "{\"name\":\"nrm_add_ex\",\"data\":[1,3.00000,4.00000,3.00000,3.00000,9."
      "00000]},"
      "{\"name\":\"nrm_force_add_ex\",\"data\":[1,5.00000,6.00000,5.00000,5."
      "00000,25.00000],\"forced\":true},"
      "{\"name\":\"nrm_add_apdex\",\"data\":[1,2,3,44.00000,44.00000,0]},"
      "{\"name\":\"nrm_force_add_apdex\",\"data\":[5,6,7,88.00000,88.00000,0],"
      "\"forced\":true}"
      "]");
  nr_free(json);

  nrm_table_destroy(&table);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_accessor_bad_parameters();
  test_find_internal_bad_parameters();
  test_find_create();
  test_add_ex();
  test_force_add_ex();
  test_add();
  test_force_add();
  test_add_apdex();
  test_force_add_apdex();
  test_add_internal();
  test_add_apdex_internal();
  test_add_bad_parameters();

  test_duplicate_metric();
  test_metric_table_to_daemon_json();
}
