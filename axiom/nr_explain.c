/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides explain plan support.
 */
#include "nr_axiom.h"

#include <stddef.h>

#include "nr_explain.h"
#include "nr_explain_private.h"
#include "util_memory.h"
#include "util_object.h"

nr_explain_plan_t* nr_explain_plan_create(void) {
  nr_explain_plan_t* plan = NULL;

  plan = (nr_explain_plan_t*)nr_zalloc(sizeof(nr_explain_plan_t));
  plan->columns = nro_new_array();
  plan->rows = nro_new_array();

  return plan;
}

void nr_explain_plan_destroy(nr_explain_plan_t** plan_ptr) {
  nr_explain_plan_t* plan;

  if (NULL == plan_ptr) {
    return;
  }

  if (NULL == *plan_ptr) {
    return;
  }

  plan = *plan_ptr;

  nro_delete(plan->columns);
  nro_delete(plan->rows);

  nr_realfree((void**)plan_ptr);
}

int nr_explain_plan_column_count(const nr_explain_plan_t* plan) {
  if (NULL == plan) {
    return 0;
  }

  return nro_getsize(plan->columns);
}

void nr_explain_plan_add_column(nr_explain_plan_t* plan, const char* name) {
  if ((NULL == plan) || (NULL == name)) {
    return;
  }

  nro_set_array_string(plan->columns, 0, name);
}

void nr_explain_plan_add_row(nr_explain_plan_t* plan, const nrobj_t* row) {
  if ((NULL == plan) || (NULL == row)) {
    return;
  }

  if (nro_getsize(row) != nro_getsize(plan->columns)) {
    return;
  }

  nro_set_array(plan->rows, 0, row);
}

char* nr_explain_plan_to_json(const nr_explain_plan_t* plan) {
  char* json = NULL;
  nrobj_t* obj = NULL;

  if (NULL == plan) {
    return NULL;
  }

  obj = nr_explain_plan_to_object(plan);
  if (NULL == obj) {
    return NULL;
  }

  json = nro_to_json(obj);
  nro_delete(obj);

  return json;
}

nrobj_t* nr_explain_plan_to_object(const nr_explain_plan_t* plan) {
  nrobj_t* obj = NULL;

  if (NULL == plan) {
    return NULL;
  }

  obj = nro_new_array();

  nro_set_array(obj, 0, plan->columns);
  nro_set_array(obj, 0, plan->rows);

  return obj;
}
