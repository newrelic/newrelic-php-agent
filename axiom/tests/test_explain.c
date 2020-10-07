/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_explain.h"
#include "nr_explain_private.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_column(void) {
  nr_explain_plan_t* plan = nr_explain_plan_create();
  int i = 0;

  /*
   * Bad parameters.
   */
  nr_explain_plan_add_column(NULL, NULL);
  nr_explain_plan_add_column(NULL, "a");
  nr_explain_plan_add_column(plan, NULL);
  tlib_pass_if_int_equal("NULL plan", 0, nr_explain_plan_column_count(NULL));

  /*
   * Good parameters.
   */
  tlib_pass_if_int_equal("empty plan", 0, nr_explain_plan_column_count(plan));

  for (i = 1; i < 10; i++) {
    nr_explain_plan_add_column(plan, "a");
    tlib_pass_if_int_equal("column count", i,
                           nr_explain_plan_column_count(plan));
  }

  nr_explain_plan_destroy(&plan);
}

static void test_destroy(void) {
  nr_explain_plan_t* plan = NULL;

  /*
   * Test that we don't segfault.
   */
  nr_explain_plan_destroy(NULL);
  nr_explain_plan_destroy(&plan);
  tlib_pass_if_null("destroy NULL", plan);

  /*
   * Test that we actually destroy the plan.
   */
  plan = nr_explain_plan_create();
  nr_explain_plan_destroy(&plan);
  tlib_pass_if_null("destroy plan", plan);
}

static void test_export(void) {
  nr_explain_plan_t* plan = NULL;
  char* json = NULL;
  char* obj_json = NULL;
  nrobj_t* obj = NULL;
  nrobj_t* row = NULL;

  /*
   * Bad parameters.
   */
  tlib_pass_if_null("NULL plan", nr_explain_plan_to_json(NULL));
  tlib_pass_if_null("NULL plan", nr_explain_plan_to_object(NULL));

  /*
   * Empty plan.
   */
  plan = nr_explain_plan_create();
  json = nr_explain_plan_to_json(plan);
  obj = nr_explain_plan_to_object(plan);
  obj_json = nro_to_json(obj);

  tlib_pass_if_str_equal("empty plan", "[[],[]]", json);
  tlib_pass_if_str_equal("empty plan", obj_json, json);

  nr_free(json);
  nr_free(obj_json);
  nro_delete(obj);
  nr_explain_plan_destroy(&plan);

  /*
   * Columns, no rows.
   */
  plan = nr_explain_plan_create();
  nr_explain_plan_add_column(plan, "a");
  nr_explain_plan_add_column(plan, "b");

  json = nr_explain_plan_to_json(plan);
  obj = nr_explain_plan_to_object(plan);
  obj_json = nro_to_json(obj);

  tlib_pass_if_str_equal("columns only", "[[\"a\",\"b\"],[]]", json);
  tlib_pass_if_str_equal("columns only", obj_json, json);

  nr_free(json);
  nr_free(obj_json);
  nro_delete(obj);
  nr_explain_plan_destroy(&plan);

  /*
   * Columns and rows.
   */
  plan = nr_explain_plan_create();
  nr_explain_plan_add_column(plan, "a");
  nr_explain_plan_add_column(plan, "b");

  row = nro_new_array();
  nro_set_array_long(row, 0, 42);
  nro_set_array_string(row, 0, "foo");
  nr_explain_plan_add_row(plan, row);
  nro_delete(row);

  row = nro_new_array();
  nro_set_array_string(row, 0, "bar");
  nro_set_array_long(row, 0, 0);
  nr_explain_plan_add_row(plan, row);
  nro_delete(row);

  json = nr_explain_plan_to_json(plan);
  obj = nr_explain_plan_to_object(plan);
  obj_json = nro_to_json(obj);

  tlib_pass_if_str_equal("columns and rows",
                         "[[\"a\",\"b\"],[[42,\"foo\"],[\"bar\",0]]]", json);
  tlib_pass_if_str_equal("columns and rows", obj_json, json);

  nr_free(json);
  nr_free(obj_json);
  nro_delete(obj);
  nr_explain_plan_destroy(&plan);
}

static void test_row(void) {
  nr_explain_plan_t* plan = NULL;
  nrobj_t* row = NULL;
  const nrobj_t* added_row = NULL;

  plan = nr_explain_plan_create();
  row = nro_new_array();

  /*
   * Bad parameters.
   */
  nr_explain_plan_add_row(NULL, NULL);
  nr_explain_plan_add_row(NULL, row);
  nr_explain_plan_add_row(plan, NULL);
  tlib_pass_if_int_equal("row count is 0", 0, nro_getsize(plan->rows));

  /*
   * Mismatched column/row count.
   */
  nr_explain_plan_add_column(plan, "a");
  nr_explain_plan_add_row(plan, row);
  tlib_pass_if_int_equal("row count is 0", 0, nro_getsize(plan->rows));

  /*
   * Actual row addition.
   */
  nro_set_array_long(row, 0, 42);
  nr_explain_plan_add_row(plan, row);
  nro_delete(row);
  tlib_pass_if_int_equal("add row", 1, nro_getsize(plan->rows));
  added_row = nro_get_array_value(plan->rows, 1, NULL);
  tlib_pass_if_int64_t_equal("add row", 42,
                             nro_get_array_long(added_row, 1, NULL));

  nr_explain_plan_destroy(&plan);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_column();
  test_destroy();
  test_export();
  test_row();
}
