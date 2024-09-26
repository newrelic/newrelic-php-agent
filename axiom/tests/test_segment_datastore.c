/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_segment_datastore.h"
#include "nr_segment_datastore_private.h"
#include "test_segment_helpers.h"

#define test_datastore_segment(...) \
  test_datastore_segment_fn(__VA_ARGS__, __FILE__, __LINE__)

#define test_datastore_segment_string(M, EXPECTED, ACTUAL)                 \
  tlib_check_if_str_equal_f((M), #EXPECTED, (EXPECTED), #ACTUAL, (ACTUAL), \
                            true, file, line)

static void test_datastore_segment_fn(nr_segment_datastore_t* datastore,
                                      const char* tname,
                                      char* component,
                                      char* sql,
                                      char* sql_obfuscated,
                                      char* input_query_json,
                                      char* backtrace_json,
                                      char* explain_plan_json,
                                      char* host,
                                      char* port_path_or_id,
                                      char* database_name,
                                      const char* file,
                                      int line) {
  test_datastore_segment_string(tname, datastore->component, component);
  test_datastore_segment_string(tname, datastore->sql, sql);
  test_datastore_segment_string(tname, datastore->sql_obfuscated,
                                sql_obfuscated);
  test_datastore_segment_string(tname, datastore->input_query_json,
                                input_query_json);
  test_datastore_segment_string(tname, datastore->backtrace_json,
                                backtrace_json);
  test_datastore_segment_string(tname, datastore->explain_plan_json,
                                explain_plan_json);
  test_datastore_segment_string(tname, datastore->instance.host, host);
  test_datastore_segment_string(tname, datastore->instance.port_path_or_id,
                                port_path_or_id);
  test_datastore_segment_string(tname, datastore->instance.database_name,
                                database_name);
}

static char* stack_dump_callback(void) {
  return nr_strdup("[\"Zip\",\"Zap\"]");
}

static nr_segment_datastore_params_t sample_segment_datastore_params(void) {
  nr_segment_datastore_params_t params
      = {.datastore = {.type = NR_DATASTORE_MONGODB},
         .collection = "my_table",
         .operation = "my_operation"};

  return params;
}

static nr_segment_datastore_params_t sample_segment_sql_params(void) {
  nr_segment_datastore_params_t params = {
      .datastore = {.type = NR_DATASTORE_MYSQL},
      .sql = {.sql = "SELECT * FROM table WHERE constant = 31"},
      .callbacks = {.backtrace = stack_dump_callback},
  };

  return params;
}

static void test_bad_parameters(void) {
  nrtxn_t* txn = new_txn(0);
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = {0};

  /*
   * Test : Bad Parameters
   */
  nr_segment_datastore_end(NULL, &params);
  nr_segment_datastore_end(&segment, NULL);
  nr_segment_datastore_end(NULL, NULL);
  segment = nr_segment_start(txn, NULL, NULL);

  nr_segment_datastore_end(&segment, NULL);
  test_txn_untouched("No start", txn);

  params.datastore.type = NR_DATASTORE_MUST_BE_LAST;
  nr_segment_datastore_end(&segment, &params);
  test_txn_untouched("bad_datastore", txn);

  txn->status.recording = 0;
  nr_segment_datastore_end(&segment, &params);
  test_txn_untouched("not recording", txn);
  txn->status.recording = 1;

  nr_txn_destroy(&txn);
}

static void test_create_metrics(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  char* tname = "create metrics";

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  /*
   * Test : Create metrics with all options
   */
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation",
                              false);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/statement/MongoDB/my_table/my_operation", true);

  nr_txn_destroy(&txn);
}

static void test_create_metrics_no_table(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  char* tname = "create metrics no table";

  /*
   * Test : Create metrics all but table
   */
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  params.collection = NULL;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation", true);

  nr_txn_destroy(&txn);
}

static void test_create_metrics_no_table_no_operation(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  const char* tname = "create metrics no table";

  params.sql.sql = "SELECT * FROM you should not see me";
  txn->options.tt_recordsql = NR_SQL_NONE;
  txn->options.ss_threshold = 1;
  txn->options.database_name_reporting_enabled = 1;
  txn->options.instance_reporting_enabled = 0;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  params.collection = NULL;
  params.operation = NULL;

  test_segment_datastore_end_and_keep(&segment, &params);

  /*
   * Test : We should still get some metrics when table and operation are
   * missing.
   */
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/other", true);

  test_datastore_segment(&segment->typed_attributes->datastore, tname,
                         "MongoDB", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         NULL);

  nr_txn_destroy(&txn);
}

static void test_create_metrics_instance_only(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_datastore_instance_t instance = {.host = "hostname",
                                      .port_path_or_id = "123",
                                      .database_name = "my database"};
  nr_segment_t* segment = NULL;
  char* tname = "create metrics";

  params.instance = &instance;
  params.instance_only = true;
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  /*
   * Test : Create only the instance metric
   */
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/instance/MongoDB/hostname/123",
                              false);

  nr_txn_destroy(&txn);
}

static void test_instance_info_reporting_disabled(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  const char* tname = "instance info reporting disabled";

  txn->options.instance_reporting_enabled = 0;
  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation",
                              false);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/statement/MongoDB/my_table/my_operation", true);

  test_datastore_segment(&segment->typed_attributes->datastore, tname,
                         "MongoDB", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
                         NULL);

  nr_txn_destroy(&txn);
  nr_datastore_instance_destroy(&params.instance);
}

static void test_instance_database_name_reporting_disabled(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  const char* tname = "instance database name reporting disabled";

  txn->options.database_name_reporting_enabled = 0;
  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 3);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation",
                              false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/instance/MongoDB/super_db_host/3306",
                              false);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/statement/MongoDB/my_table/my_operation", true);

  test_datastore_segment(&segment->typed_attributes->datastore, tname,
                         "MongoDB", NULL, NULL, NULL, NULL, NULL,
                         "super_db_host", "3306", NULL);

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

static void test_instance_info_empty(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  const char* tname = "instance info empty";

  params.instance = nr_datastore_instance_create("", "", "");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 3);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation",
                              false);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/statement/MongoDB/my_table/my_operation", true);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/instance/MongoDB/unknown/unknown",
                              false);

  test_datastore_segment(&segment->typed_attributes->datastore, tname,
                         "MongoDB", NULL, NULL, NULL, NULL, NULL, "unknown",
                         "unknown", "unknown");

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

static void test_instance_metric_with_slashes(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_datastore_params();
  nr_segment_t* segment = NULL;
  const char* tname = "instance metric with slashes";

  params.instance = nr_datastore_instance_create(
      "super_db_host", "/path/to/socket", "my_database");
  txn->options.database_name_reporting_enabled = 1;
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MongoDB/all");
  test_metric_vector_size(segment->metrics, 3);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MongoDB/my_operation",
                              false);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/statement/MongoDB/my_table/my_operation", true);
  test_segment_metric_created(
      tname, segment->metrics,
      "Datastore/instance/MongoDB/super_db_host//path/to/socket", false);

  test_datastore_segment(&segment->typed_attributes->datastore, tname,
                         "MongoDB", NULL, NULL, NULL, NULL, NULL,
                         "super_db_host", "/path/to/socket", "my_database");

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

#define EXPLAIN_PLAN_JSON "[[\"a\",\"b\"],[[1,2],[3,4]]]"
static void test_value_transforms(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 100;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  const char* tname = "value transforms";

  params.sql.plan_json = EXPLAIN_PLAN_JSON;

  txn->options.ep_threshold = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;
  txn->options.ss_threshold = 1;
  txn->options.database_name_reporting_enabled = 1;

  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         "SELECT * FROM table WHERE constant = 31", NULL, NULL,
                         "[\"Zip\",\"Zap\"]", EXPLAIN_PLAN_JSON,
                         "super_db_host", "3306", "my_database");

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

static void test_web_transaction_commit(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_t* segment = NULL;
  const char* tname = "web transaction mysqli::commit";

  nr_segment_datastore_params_t params = {
      .datastore = {.type = NR_DATASTORE_MYSQL},
      .sql = {.sql = "commit"},
      .callbacks = {.backtrace = stack_dump_callback},
  };

  txn->status.recording = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;
  txn->options.database_name_reporting_enabled = 1;
  txn->options.instance_reporting_enabled = 0;

  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "commit", NULL, "[\"Zip\",\"Zap\"]", NULL, NULL,
                         NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/commit", true);

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

static void test_web_transaction_insert(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  const char* tname = "web transaction insert";

  txn->status.recording = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;
  txn->options.database_name_reporting_enabled = 1;
  txn->options.instance_reporting_enabled = 0;

  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?", NULL,
                         "[\"Zip\",\"Zap\"]", NULL, NULL, NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}

static void test_options_tt_recordsql_obeyed_part0(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  const char* tname = "options tt recordsql obeyed part0";

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;
  txn->status.recording = 1;
  txn->options.tt_recordsql = NR_SQL_NONE;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  nr_txn_destroy(&txn);
}

static void test_options_tt_recordsql_obeyed_part1(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  char* tname = "options tt_recordsql obeyed part1";
  const char* name = NULL;

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;
  txn->status.recording = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         "SELECT * FROM table WHERE constant = 31", NULL, NULL,
                         NULL, NULL, NULL, NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  name = nr_string_get(txn->trace_strings, segment->name);

  tlib_pass_if_str_equal(tname, "Datastore/statement/MySQL/table/select", name);

  nr_txn_destroy(&txn);
}

static void test_options_tt_recordsql_obeyed_part2(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  char* tname = "options tt_recordsql obeyed part 2";

  txn->options.ss_threshold = duration + 1;
  txn->options.ep_threshold = duration + 1;
  txn->status.recording = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?", NULL,
                         NULL, NULL, NULL, NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  nr_txn_destroy(&txn);
}

static void test_options_high_security_tt_recordsql_raw(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  char* tname = "options high security - raw";

  txn->options.ss_threshold = duration + 3;
  txn->options.ep_threshold = duration + 3;
  txn->status.recording = 1;
  txn->high_security = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?", NULL,
                         NULL, NULL, NULL, NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  nr_txn_destroy(&txn);
}

static void test_stack_recorded(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  char* tname = "stack recorded";

  txn->options.ss_threshold = 1;
  txn->status.recording = 1;
  txn->options.tt_recordsql = NR_SQL_NONE;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, NULL, NULL, "[\"Zip\",\"Zap\"]", NULL, NULL,
                         NULL, NULL);

  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  nr_txn_destroy(&txn);
}

static void test_slowsql_raw_saved(void) {
  const char* tname = "raw slowsql saved";
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;
  const nr_slowsql_t* slow;

  txn->status.recording = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;
  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         "SELECT * FROM table WHERE constant = 31", NULL, NULL,
                         "[\"Zip\",\"Zap\"]", NULL, NULL, NULL, NULL);

  slow = nr_slowsqls_at(txn->slowsqls, 0);
  tlib_pass_if_uint32_t_equal(tname, nr_slowsql_id(slow), 3202261176);
  tlib_pass_if_int_equal(tname, nr_slowsql_count(slow), 1);

  tlib_pass_if_true(tname, 4000000 <= nr_slowsql_min(slow),
                    "nr_slowsql_min(slow) = %d", (int)nr_slowsql_min(slow));
  tlib_pass_if_true(tname, 4000000 <= nr_slowsql_max(slow),
                    "nr_slowsql_max(slow) = %d", (int)nr_slowsql_max(slow));
  tlib_pass_if_true(tname, 4000000 <= nr_slowsql_total(slow),
                    "nr_slowsql_total(slow) = %d", (int)nr_slowsql_total(slow));
  tlib_pass_if_str_equal(tname, nr_slowsql_metric(slow),
                         "Datastore/statement/MySQL/table/select");
  tlib_pass_if_str_equal(tname, nr_slowsql_query(slow),
                         "SELECT * FROM table WHERE constant = 31");
  tlib_pass_if_str_equal(tname, nr_slowsql_params(slow),
                         "{\"backtrace\":[\"Zip\",\"Zap\"]}");

  nr_txn_destroy(&txn);
}

static void test_table_not_found(void) {
  const char* tname = "table not found";
  const char* name;
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;

  params.sql.sql = "SELECT";
  txn->status.recording = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT", NULL, "[\"Zip\",\"Zap\"]", NULL, NULL,
                         NULL, NULL);
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", true);

  name = nr_string_get(txn->trace_strings, segment->name);

  tlib_pass_if_str_equal(tname, "Datastore/operation/MySQL/select", name);

  nr_txn_destroy(&txn);
}

static void test_table_and_operation_not_found(void) {
  const char* tname = "table and operation not found";
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 4 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;

  params.sql.sql = "*";
  txn->status.recording = 1;
  txn->options.ep_threshold = 1;
  txn->options.ss_threshold = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;

  test_segment_datastore_end_and_keep(&segment, &params);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "*", NULL, "[\"Zip\",\"Zap\"]", NULL, NULL, NULL,
                         NULL);
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 1);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/other", true);

  nr_txn_destroy(&txn);
}

nr_slowsqls_labelled_query_t sample_input_query = {
    .name = "Doctrine DQL Query",
    .query = "SELECT COUNT(b) from Bot b where b.size = 23;",
};

static void test_input_query_raw(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  const nr_slowsql_t* slowsql;
  const char* tname = "raw input query";
  nr_segment_t* segment = NULL;

  params.sql.input_query = &sample_input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->status.recording = 1;
  txn->options.tt_slowsql = 1;
  txn->options.tt_recordsql = NR_SQL_RAW;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);
  slowsql = nr_slowsqls_at(txn->slowsqls, 0);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         "SELECT * FROM table WHERE constant = 31", NULL,
                         "{\"label\":\"Doctrine DQL Query\",\"query\":\"SELECT "
                         "COUNT(b) from Bot b where b.size = 23;\"}",
                         "[\"Zip\",\"Zap\"]", NULL, NULL, NULL, NULL);
  test_metric_table_size(tname, txn->unscoped_metrics, 2);
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/all");
  test_metric_created(tname, txn->unscoped_metrics, MET_FORCED, duration,
                      "Datastore/MySQL/all");
  test_metric_vector_size(segment->metrics, 2);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/operation/MySQL/select", false);
  test_segment_metric_created(tname, segment->metrics,
                              "Datastore/statement/MySQL/table/select", true);

  tlib_pass_if_str_equal(
      tname, nr_slowsql_params(slowsql),
      "{\"backtrace\":[\"Zip\",\"Zap\"],"
      "\"input_query\":{"
      "\"label\":\"Doctrine DQL Query\","
      "\"query\":\"SELECT COUNT(b) from Bot b where b.size = 23;\"}}");
  nr_txn_destroy(&txn);
}

static void test_input_query_empty(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  const nr_slowsql_t* slowsql;
  const char* tname = "input query empty";
  nr_slowsqls_labelled_query_t input_query;
  nr_segment_t* segment = NULL;

  input_query.name = "";
  input_query.query = "";
  params.sql.input_query = &input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);
  slowsql = nr_slowsqls_at(txn->slowsqls, 0);

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?",
                         "{\"label\":\"\",\"query\":\"\"}", "[\"Zip\",\"Zap\"]",
                         NULL, NULL, NULL, NULL);

  tlib_pass_if_str_equal(tname, nr_slowsql_params(slowsql),
                         "{\"backtrace\":[\"Zip\",\"Zap\"],"
                         "\"input_query\":{"
                         "\"label\":\"\","
                         "\"query\":\"\"}}");

  nr_txn_destroy(&txn);
}
static void test_input_query_null_fields(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  const nr_slowsql_t* slowsql;
  const char* tname = "input query NULL fields";
  nr_slowsqls_labelled_query_t input_query;
  nr_segment_t* segment = NULL;

  input_query.name = NULL;
  input_query.query = NULL;
  params.sql.input_query = &input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);
  slowsql = nr_slowsqls_at(txn->slowsqls, 0);

  tlib_pass_if_str_equal(tname, nr_slowsql_params(slowsql),
                         "{\"backtrace\":[\"Zip\",\"Zap\"],"
                         "\"input_query\":{"
                         "\"label\":\"\","
                         "\"query\":\"\"}}");

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?",
                         "{\"label\":\"\",\"query\":\"\"}", "[\"Zip\",\"Zap\"]",
                         NULL, NULL, NULL, NULL);
  nr_txn_destroy(&txn);
}

static void test_input_query_obfuscated(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  const nr_slowsql_t* slowsql;
  const char* tname = "input query obfuscated";
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;

  params.sql.input_query = &sample_input_query;
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);
  slowsql = nr_slowsqls_at(txn->slowsqls, 0);

  tlib_pass_if_str_equal(
      tname, nr_slowsql_params(slowsql),
      "{\"backtrace\":[\"Zip\",\"Zap\"],"
      "\"input_query\":{"
      "\"label\":\"Doctrine DQL Query\","
      "\"query\":\"SELECT COUNT(b) from Bot b where b.size = ?;\"}}");
  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?",
                         "{\"label\":\"Doctrine DQL Query\",\"query\":\"SELECT "
                         "COUNT(b) from Bot b where b.size = ?;\"}",
                         "[\"Zip\",\"Zap\"]", NULL, NULL, NULL, NULL);

  nr_datastore_instance_destroy(&params.instance);
  nr_txn_destroy(&txn);
}
static void test_instance_info_present(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  const nr_slowsql_t* slowsql;
  const char* tname = "instance info present";
  nr_segment_datastore_params_t params = sample_segment_sql_params();
  nr_segment_t* segment = NULL;

  params.instance
      = nr_datastore_instance_create("super_db_host", "3306", "my_database");
  txn->options.ep_threshold = 0;
  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 1;
  txn->options.database_name_reporting_enabled = 1;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);
  slowsql = nr_slowsqls_at(txn->slowsqls, 0);

  tlib_pass_if_str_equal(tname, nr_slowsql_params(slowsql),
                         "{\"backtrace\":[\"Zip\",\"Zap\"],"
                         "\"host\":\"super_db_host\","
                         "\"port_path_or_id\":\"3306\","
                         "\"database_name\":\"my_database\"}");

  test_datastore_segment(&segment->typed_attributes->datastore, tname, "MySQL",
                         NULL, "SELECT * FROM table WHERE constant = ?", NULL,
                         "[\"Zip\",\"Zap\"]", NULL, "super_db_host", "3306",
                         "my_database");

  nr_txn_destroy(&txn);
  nr_datastore_instance_destroy(&params.instance);
}

static void test_no_datastore_type(void) {
  nrtxn_t* txn = new_txn(0);
  nrtime_t duration = 1 * NR_TIME_DIVISOR;
  nr_segment_datastore_params_t params
      = {.collection = "my_table", .operation = "my_operation"};
  nr_segment_t* segment = NULL;

  segment = nr_segment_start(txn, NULL, NULL);
  segment->start_time = 1 * NR_TIME_DIVISOR;
  segment->stop_time = 1 * NR_TIME_DIVISOR + duration;
  test_segment_datastore_end_and_keep(&segment, &params);

  tlib_pass_if_ptr_equal("typed attributes uninitialized", NULL,
                         segment->typed_attributes);

  nr_txn_destroy(&txn);
}

static void modify_table_name(char* tablename) {
  if (0 == nr_strcmp(tablename, "fix_me")) {
    tablename[3] = '\0';
  }
}

static void test_get_operation_and_table(void) {
  char* table;
  const char* operation = NULL;
  nrtxn_t txn;
  const char* sql = "SELECT * FROM MY_TABLE";

  txn.special_flags.no_sql_parsing = 0;
  txn.special_flags.show_sql_parsing = 0;
  operation = NULL;

  table = nr_segment_sql_get_operation_and_table(NULL, &operation, sql,
                                                 &modify_table_name);
  tlib_pass_if_null("null txn", table);
  tlib_pass_if_null("null txn", operation);

  txn.special_flags.no_sql_parsing = 1;
  table = nr_segment_sql_get_operation_and_table(&txn, &operation, sql,
                                                 &modify_table_name);
  tlib_pass_if_null("no_sql_parsing", table);
  tlib_pass_if_null("no_sql_parsing", operation);
  txn.special_flags.no_sql_parsing = 0;

  table = nr_segment_sql_get_operation_and_table(&txn, &operation, sql,
                                                 &modify_table_name);
  tlib_pass_if_str_equal("success", table, "MY_TABLE");
  tlib_pass_if_str_equal("success", operation, "select");
  nr_free(table);

  operation = NULL;
  table = nr_segment_sql_get_operation_and_table(&txn, &operation, "SELECT *",
                                                 &modify_table_name);
  tlib_pass_if_null("no table found", table);
  tlib_pass_if_str_equal("no table found", operation, "select");
  nr_free(table);

  operation = NULL;
  table = nr_segment_sql_get_operation_and_table(
      &txn, &operation, "SELECT * FROM fix_me", &modify_table_name);
  tlib_pass_if_str_equal("table modified", table, "fix");
  tlib_pass_if_str_equal("table modified", operation, "select");
  nr_free(table);
}

static void test_segment_stack_worthy(void) {
  int rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->options.ss_threshold = 0;
  txn->options.tt_slowsql = 0;
  txn->options.ep_threshold = 0;

  rv = nr_segment_datastore_stack_worthy(0, 0);
  tlib_pass_if_true("zero params", 0 == rv, "rv=%d", rv);

  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("all options zero", 0 == rv, "rv=%d", rv);

  txn->options.ss_threshold = 5;
  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("above ss_threshold", 1 == rv, "rv=%d", rv);

  txn->options.ss_threshold = 15;
  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("below ss_threshold", 0 == rv, "rv=%d", rv);
  txn->options.ss_threshold = 0;

  txn->options.ep_threshold = 5;
  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("non-zero ep_threshold tt_slowsql disabled", 0 == rv,
                    "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 15;
  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("below ep_threshold", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_segment_datastore_stack_worthy(txn, 10);
  tlib_pass_if_true("success", 1 == rv, "rv=%d", rv);
}

static void test_segment_potential_explain_plan(void) {
  int rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->options.tt_slowsql = 1;
  txn->options.ep_enabled = 0;
  txn->options.ep_threshold = 15;
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;

  rv = nr_segment_potential_explain_plan(NULL, 0);
  tlib_pass_if_int_equal("NULL txn", 0, rv);

  rv = nr_segment_potential_explain_plan(txn, 20);
  tlib_pass_if_int_equal("explain plan disabled", 0, rv);

  txn->options.ep_enabled = 1;

  rv = nr_segment_potential_explain_plan(txn, 10);
  tlib_pass_if_int_equal("explain plan below threshold", 0, rv);

  rv = nr_segment_potential_explain_plan(txn, 20);
  tlib_fail_if_int_equal("explain plan enabled", 0, rv);
}

static void test_segment_potential_slowsql(void) {
  int rv;
  nrtxn_t txnv;
  nrtxn_t* txn = &txnv;

  txn->options.tt_slowsql = 0;
  txn->options.ep_threshold = 0;
  txn->options.tt_recordsql = NR_SQL_OBFUSCATED;

  rv = nr_segment_potential_slowsql(0, 0);
  tlib_pass_if_true("zero params", 0 == rv, "rv=%d", rv);

  rv = nr_segment_potential_slowsql(txn, 10);
  tlib_pass_if_true("all options zero", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_segment_potential_slowsql(txn, 10);
  tlib_pass_if_true("non-zero ep_threshold tt_slowsql disabled", 0 == rv,
                    "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 15;
  rv = nr_segment_potential_slowsql(txn, 10);
  tlib_pass_if_true("below ep_threshold", 0 == rv, "rv=%d", rv);

  txn->options.ep_threshold = 5;
  rv = nr_segment_potential_slowsql(txn, 10);
  tlib_pass_if_true("success", 1 == rv, "rv=%d", rv);

  txn->options.tt_slowsql = 1;
  txn->options.ep_threshold = 5;
  txn->options.tt_recordsql = NR_SQL_NONE;
  rv = nr_segment_potential_slowsql(txn, 10);
  tlib_pass_if_true("sql recording off", 0 == rv, "rv=%d", rv);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_bad_parameters();
  test_create_metrics();
  test_create_metrics_instance_only();
  test_create_metrics_no_table();
  test_create_metrics_no_table_no_operation();
  test_instance_info_reporting_disabled();
  test_instance_database_name_reporting_disabled();
  test_instance_info_empty();
  test_instance_metric_with_slashes();
  test_value_transforms();
  test_web_transaction_commit();
  test_web_transaction_insert();
  test_options_tt_recordsql_obeyed_part0();
  test_options_tt_recordsql_obeyed_part1();
  test_options_tt_recordsql_obeyed_part2();
  test_options_high_security_tt_recordsql_raw();
  test_stack_recorded();
  test_slowsql_raw_saved();
  test_table_not_found();
  test_table_and_operation_not_found();
  test_input_query_raw();
  test_input_query_empty();
  test_input_query_null_fields();
  test_input_query_obfuscated();
  test_instance_info_present();
  test_get_operation_and_table();
  test_segment_stack_worthy();
  test_segment_potential_explain_plan();
  test_segment_potential_slowsql();
  test_no_datastore_type();
}
