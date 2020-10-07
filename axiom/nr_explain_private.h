/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides explain plan support.
 */
#ifndef NR_EXPLAIN_PRIVATE_HDR
#define NR_EXPLAIN_PRIVATE_HDR

#include "nr_explain.h"
#include "util_object.h"

/*
 * An explain plan, which is represented as a result set of rows with column
 * names for the New Relic backend to puzzle over.
 */
struct _nr_explain_plan_t {
  nrobj_t* columns;
  nrobj_t* rows;
};

/*
 * Purpose : Exports an explain plan into an abstract object.
 *
 * Params  : 1. The explain plan.
 *
 * Returns : An abstract object, which will need to be deleted with nro_delete
 *           when no longer needed.
 */
extern nrobj_t* nr_explain_plan_to_object(const nr_explain_plan_t* plan);

#endif /* NR_EXPLAIN_PRIVATE_HDR */
