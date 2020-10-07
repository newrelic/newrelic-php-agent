/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides explain plan support.
 */
#ifndef NR_EXPLAIN_HDR
#define NR_EXPLAIN_HDR

#include "util_object.h"

/*
 * Forward declarations of the opaque structure used for explain plans.
 */
typedef struct _nr_explain_plan_t nr_explain_plan_t;

/*
 * Purpose : Creates a new explain plan structure, which is effectively a fancy
 *           structure to represent the result set returned by a database for
 *           an EXPLAIN query.
 *
 * Returns : A pointer to an nr_explain_plan_t structure, which will need to be
 *           destroyed with nr_explain_plan_destroy when no longer needed.
 */
extern nr_explain_plan_t* nr_explain_plan_create(void);

/*
 * Purpose : Destroys an explain plan structure.
 *
 * Params  : 1. A pointer to a pointer to a plan structure.
 */
extern void nr_explain_plan_destroy(nr_explain_plan_t** plan_ptr);

/*
 * Purpose : Returns the number of columns defined in the explain plan.
 *
 * Params  : 1. The explain plan.
 *
 * Returns : The number of columns added via nr_explain_plan_add_column.
 */
extern int nr_explain_plan_column_count(const nr_explain_plan_t* plan);

/*
 * Purpose : Adds a column to the explain plan.
 *
 * Params  : 1. The explain plan.
 *           2. The name of the column.
 */
extern void nr_explain_plan_add_column(nr_explain_plan_t* plan,
                                       const char* name);

/*
 * Purpose : Adds a row to the explain plan.
 *
 * Params  : 1. The explain plan.
 *           2. The row, which should be an array containing the same number of
 *              elements as there are columns in the explain plan.
 *
 * Note    : The row may be deleted after calling nr_explain_plan_add_row; this
 *           function copies the values within the row and does not cause the
 *           explain plan to take ownership of the row.
 */
extern void nr_explain_plan_add_row(nr_explain_plan_t* plan,
                                    const nrobj_t* row);

/*
 * Purpose : Exports an explain plan into JSON to be sent to the collector.
 *
 * Params  : 1. The explain plan.
 *
 * Returns : A JSON string, which will need to be freed when no longer needed.
 */
extern char* nr_explain_plan_to_json(const nr_explain_plan_t* plan);

#endif /* NR_EXPLAIN_HDR */
