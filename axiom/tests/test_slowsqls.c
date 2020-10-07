/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "nr_slowsqls.h"
#include "util_memory.h"
#include "util_sql.h"
#include "util_strings.h"
#include "util_time.h"

#include "tlib_main.h"

static nr_slowsqls_params_t sample_slowsql_params(void) {
  nr_slowsqls_params_t params = {
      .sql = "my/sql",
      .duration = 5 * NR_TIME_DIVISOR,
      .stacktrace_json = "[\"already\\/escaped\"]",
      .metric_name = "my/metric",
      .instance_reporting_enabled = 1,
      .database_name_reporting_enabled = 1,
  };

  return params;
}

static void test_simple_add(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);
  nr_slowsqls_add(slowsqls, &params);
  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("simply add", nr_slowsql_id(slow), 2902036434);
  tlib_pass_if_int_equal("simply add", nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal("simply add", nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal("simply add", nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal("simply add", nr_slowsql_total(slow), 5000000);
  tlib_pass_if_str_equal("simply add", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("simply add", nr_slowsql_query(slow), "my/sql");
  tlib_pass_if_str_equal("simply add", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slowsqls);
}

static void test_min_max(void) {
  nr_slowsqls_t* slowsqls = nr_slowsqls_create(1);
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  params.duration = 5 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);
  params.duration = 3 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);
  params.duration = 4 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);
  params.duration = 6 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);
  params.duration = 7 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);
  params.duration = 5 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slowsqls, &params);

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("test min/max", nr_slowsql_id(slow), 2902036434);
  tlib_pass_if_int_equal("test min/max", nr_slowsql_count(slow), 6);
  tlib_pass_if_time_equal("test min/max", nr_slowsql_min(slow), 3000000);
  tlib_pass_if_time_equal("test min/max", nr_slowsql_max(slow), 7000000);
  tlib_pass_if_time_equal("test min/max", nr_slowsql_total(slow), 30000000);
  tlib_pass_if_str_equal("test min/max", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("test min/max", nr_slowsql_query(slow), "my/sql");
  tlib_pass_if_str_equal("test min/max", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slowsqls);
}

static void test_raw_sql_aggregation(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);
  params.sql = "SELECT * FROM test WHERE foo IN (1)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (2)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (2, 3, 4)";
  nr_slowsqls_add(slowsqls, &params);

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("simple raw aggregation", nr_slowsql_id(slow),
                              2676686092);
  tlib_pass_if_int_equal("simple raw aggregation", nr_slowsql_count(slow), 3);
  tlib_pass_if_time_equal("simple raw aggregation", nr_slowsql_min(slow),
                          5000000);
  tlib_pass_if_time_equal("simple raw aggregation", nr_slowsql_max(slow),
                          5000000);
  tlib_pass_if_time_equal("simple raw aggregation", nr_slowsql_total(slow),
                          15000000);
  tlib_pass_if_str_equal("simple raw aggregation", nr_slowsql_metric(slow),
                         "my/metric");
  tlib_pass_if_str_equal("simple raw aggregation", nr_slowsql_query(slow),
                         "SELECT * FROM test WHERE foo IN (1)");
  tlib_pass_if_str_equal("simple raw aggregation", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slowsqls);
}

static void test_obfuscated_sql_aggregation(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);
  params.sql = "SELECT * FROM test WHERE foo IN (?)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (?)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (?, ?, ?)";
  nr_slowsqls_add(slowsqls, &params);

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("obfuscated", nr_slowsql_id(slow), 2676686092);
  tlib_pass_if_int_equal("obfuscated", nr_slowsql_count(slow), 3);
  tlib_pass_if_time_equal("obfuscated", nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal("obfuscated", nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal("obfuscated", nr_slowsql_total(slow), 15000000);
  tlib_pass_if_str_equal("obfuscated", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("obfuscated", nr_slowsql_query(slow),
                         "SELECT * FROM test WHERE foo IN (?)");
  tlib_pass_if_str_equal("obfuscated", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slowsqls);
}

static void test_mixed_aggregation(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);
  params.sql = "SELECT * FROM test WHERE foo IN (?)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (9)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (?, ?, ?)";
  nr_slowsqls_add(slowsqls, &params);
  params.sql = "SELECT * FROM test WHERE foo IN (9, 9, 9, ?)";
  nr_slowsqls_add(slowsqls, &params);

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("mixed aggregation", nr_slowsql_id(slow),
                              2676686092);
  tlib_pass_if_int_equal("mixed aggregation", nr_slowsql_count(slow), 4);
  tlib_pass_if_time_equal("mixed aggregation", nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal("mixed aggregation", nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal("mixed aggregation", nr_slowsql_total(slow),
                          20000000);
  tlib_pass_if_str_equal("mixed aggregation", nr_slowsql_metric(slow),
                         "my/metric");
  tlib_pass_if_str_equal("mixed aggregation", nr_slowsql_query(slow),
                         "SELECT * FROM test WHERE foo IN (?)");
  tlib_pass_if_str_equal("mixed aggregation", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slowsqls);
}

static void test_slowest_taken(void) {
  int i;
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  nr_slowsqls_params_t params = sample_slowsql_params();

  /*
   * Add in increasing order
   */
  slowsqls = nr_slowsqls_create(2);
  for (i = 0; i < 10; i++) {
    char sql[128];

    snprintf(sql, sizeof(sql), "my/sql/%c", 'a' + i);
    params.duration = (i + 1) * NR_TIME_DIVISOR;
    params.sql = sql;
    nr_slowsqls_add(slowsqls, &params);
  }

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("slowest taken", nr_slowsql_id(slow), 983362361);
  tlib_pass_if_int_equal("slowest taken", nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_min(slow), 9000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_max(slow), 9000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_total(slow), 9000000);
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_query(slow), "my/sql/i");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");
  slow = nr_slowsqls_at(slowsqls, 1);
  tlib_pass_if_uint32_t_equal("slowest taken", nr_slowsql_id(slow), 1860598843);
  tlib_pass_if_int_equal("slowest taken", nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_min(slow), 10000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_max(slow), 10000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_total(slow), 10000000);
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_query(slow), "my/sql/j");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");
  nr_slowsqls_destroy(&slowsqls);

  /*
   * Add in decreasing order
   */
  slowsqls = nr_slowsqls_create(2);
  for (i = 9; i >= 0; i--) {
    char sql[128];

    snprintf(sql, sizeof(sql), "my/sql/%c", 'a' + i);
    params.duration = (i + 1) * NR_TIME_DIVISOR;
    params.sql = sql;
    nr_slowsqls_add(slowsqls, &params);
  }

  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal("slowest taken", nr_slowsql_id(slow), 1860598843);
  tlib_pass_if_int_equal("slowest taken", nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_min(slow), 10000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_max(slow), 10000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_total(slow), 10000000);
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_query(slow), "my/sql/j");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");
  slow = nr_slowsqls_at(slowsqls, 1);
  tlib_pass_if_uint32_t_equal("slowest taken", nr_slowsql_id(slow), 983362361);
  tlib_pass_if_int_equal("slowest taken", nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_min(slow), 9000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_max(slow), 9000000);
  tlib_pass_if_time_equal("slowest taken", nr_slowsql_total(slow), 9000000);
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_query(slow), "my/sql/i");
  tlib_pass_if_str_equal("slowest taken", nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");
  nr_slowsqls_destroy(&slowsqls);
}

static void test_max_time_ranked(void) {
  nr_slowsqls_t* slows = nr_slowsqls_create(1);
  const nr_slowsql_t* slow;
  const char* testname = "max time, not total time, determines what is saved";
  nr_slowsqls_params_t params = sample_slowsql_params();

  /* sql/one has largest total, but not largest max */
  params.sql = "sql/one";
  params.duration = 2 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slows, &params);
  nr_slowsqls_add(slows, &params);
  nr_slowsqls_add(slows, &params);
  nr_slowsqls_add(slows, &params);

  /* sql/two has smallest total, but largest max */
  params.sql = "sql/two";
  params.duration = 5 * NR_TIME_DIVISOR;
  nr_slowsqls_add(slows, &params);

  slow = nr_slowsqls_at(slows, 0);
  tlib_pass_if_uint32_t_equal(testname, nr_slowsql_id(slow), 917616874);
  tlib_pass_if_int_equal(testname, nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal(testname, nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_total(slow), 5000000);
  tlib_pass_if_str_equal(testname, nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal(testname, nr_slowsql_query(slow), "sql/two");
  tlib_pass_if_str_equal(testname, nr_slowsql_params(slow),
                         "{\"backtrace\":[\"already\\/escaped\"]}");

  nr_slowsqls_destroy(&slows);
}

static void test_destroy_bad_params(void) {
  nr_slowsqls_t* slowsqls = 0;

  /* Don't blow up! */
  nr_slowsqls_destroy(0);
  nr_slowsqls_destroy(&slowsqls);
}

static void test_create_bad_params(void) {
  nr_slowsqls_t* slowsqls;

  slowsqls = nr_slowsqls_create(0);
  tlib_pass_if_true("create zero max", 0 == slowsqls, "slowsqls=%p", slowsqls);
  slowsqls = nr_slowsqls_create(-1);
  tlib_pass_if_true("create -1 max", 0 == slowsqls, "slowsqls=%p", slowsqls);
}

static void test_add_bad_params(void) {
  nr_slowsqls_t* slowsqls;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);

  /* No slowsqls: Don't blow up! */
  nr_slowsqls_add(0, 0);
  nr_slowsqls_add(0, &params);

  /* No params */
  nr_slowsqls_add(slowsqls, NULL);

  /* No stacktrace */
  params = sample_slowsql_params();
  params.stacktrace_json = NULL;
  nr_slowsqls_add(slowsqls, &params);

  /* No SQL */
  params = sample_slowsql_params();
  params.sql = NULL;
  nr_slowsqls_add(slowsqls, &params);

  /* Empty SQL */
  params = sample_slowsql_params();
  params.sql = "";
  nr_slowsqls_add(slowsqls, &params);

  /* No Duration */
  params = sample_slowsql_params();
  params.duration = 0;
  nr_slowsqls_add(slowsqls, &params);

  /* No Metric */
  params = sample_slowsql_params();
  params.metric_name = NULL;
  nr_slowsqls_add(slowsqls, &params);

  tlib_pass_if_int_equal("bad params", nr_slowsqls_saved(slowsqls), 0);

  nr_slowsqls_destroy(&slowsqls);
}

static void test_add_all_hash_params(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  const char* testname = "with explain plan, input query, and instance";
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);

  params.plan_json = "[[\"foo\",\"bar\"],[[1,2]]]";
  params.input_query_json = "{\"label\":\"zip\",\"query\":\"zap\"}";
  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");

  nr_slowsqls_add(slowsqls, &params);
  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal(testname, nr_slowsql_id(slow), 2902036434);
  tlib_pass_if_int_equal(testname, nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal(testname, nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_total(slow), 5000000);
  tlib_pass_if_str_equal(testname, nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal(testname, nr_slowsql_query(slow), "my/sql");
  tlib_pass_if_str_equal(
      testname, nr_slowsql_params(slow),
      "{"
      "\"explain_plan\":[[\"foo\",\"bar\"],[[1,2]]],"
      "\"backtrace\":[\"already\\/escaped\"],"
      "\"input_query\":{\"label\":\"zip\",\"query\":\"zap\"},"
      "\"host\":\"super_db_host\","
      "\"port_path_or_id\":\"3306\","
      "\"database_name\":\"my_database\""
      "}");
  nr_slowsqls_destroy(&slowsqls);
  nr_datastore_instance_destroy(&params.instance);
}

static void test_instance_info_disabled(void) {
  nr_slowsqls_t* slowsqls;
  const nr_slowsql_t* slow;
  const char* testname = "with instance info disabled";
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(1);

  params.plan_json = "[[\"foo\",\"bar\"],[[1,2]]]";
  params.input_query_json = "{\"label\":\"zip\",\"query\":\"zap\"}";
  params.instance = nr_datastore_instance_create("does", "not", "matter");
  params.instance_reporting_enabled = 0;
  params.database_name_reporting_enabled = 0;

  nr_slowsqls_add(slowsqls, &params);
  slow = nr_slowsqls_at(slowsqls, 0);
  tlib_pass_if_uint32_t_equal(testname, nr_slowsql_id(slow), 2902036434);
  tlib_pass_if_int_equal(testname, nr_slowsql_count(slow), 1);
  tlib_pass_if_time_equal(testname, nr_slowsql_min(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_max(slow), 5000000);
  tlib_pass_if_time_equal(testname, nr_slowsql_total(slow), 5000000);
  tlib_pass_if_str_equal(testname, nr_slowsql_metric(slow), "my/metric");
  tlib_pass_if_str_equal(testname, nr_slowsql_query(slow), "my/sql");
  tlib_pass_if_str_equal(testname, nr_slowsql_params(slow),
                         "{"
                         "\"explain_plan\":[[\"foo\",\"bar\"],[[1,2]]],"
                         "\"backtrace\":[\"already\\/escaped\"],"
                         "\"input_query\":{\"label\":\"zip\",\"query\":\"zap\"}"
                         "}");
  nr_slowsqls_destroy(&slowsqls);
  nr_datastore_instance_destroy(&params.instance);
}

static void test_slowsqls_at_bad_params(void) {
  nr_slowsqls_t* slowsqls;
  nr_slowsqls_params_t params = sample_slowsql_params();

  slowsqls = nr_slowsqls_create(10);

  nr_slowsqls_add(slowsqls, &params);

  tlib_pass_if_null("null ptr", nr_slowsqls_at(NULL, 1));
  tlib_pass_if_null("negative idx", nr_slowsqls_at(slowsqls, -1));
  tlib_pass_if_null("large idx", nr_slowsqls_at(slowsqls, 1));
  tlib_pass_if_not_null("success", nr_slowsqls_at(slowsqls, 0));

  nr_slowsqls_destroy(&slowsqls);
}

static void test_slowsql_accessor_bad_params(void) {
  tlib_pass_if_uint32_t_equal("null sql", nr_slowsql_id(NULL), 0);
  tlib_pass_if_int_equal("null sql", nr_slowsql_count(NULL), 0);
  tlib_pass_if_time_equal("null sql", nr_slowsql_min(NULL), 0);
  tlib_pass_if_time_equal("null sql", nr_slowsql_max(NULL), 0);
  tlib_pass_if_time_equal("null sql", nr_slowsql_total(NULL), 0);
  tlib_pass_if_str_equal("null sql", nr_slowsql_metric(NULL), NULL);
  tlib_pass_if_str_equal("null sql", nr_slowsql_query(NULL), NULL);
  tlib_pass_if_str_equal("null sql", nr_slowsql_params(NULL), NULL);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_simple_add();
  test_min_max();
  test_raw_sql_aggregation();
  test_obfuscated_sql_aggregation();
  test_mixed_aggregation();
  test_slowest_taken();
  test_max_time_ranked();
  test_destroy_bad_params();
  test_create_bad_params();
  test_add_bad_params();
  test_add_all_hash_params();
  test_instance_info_disabled();
  test_slowsqls_at_bad_params();
  test_slowsql_accessor_bad_params();
}
