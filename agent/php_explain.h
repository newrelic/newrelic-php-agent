/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides utility functions for generating explain plans.
 */
#ifndef PHP_EXPLAIN_HDR
#define PHP_EXPLAIN_HDR

#include "nr_explain.h"

/*
 * Purpose : Add a value to an explain plan row.
 *
 * Params  : 1. The zval to add.
 *           2. The row to add the zval to.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_explain_add_value_to_row(const zval* zv,
                                                   nrobj_t* row);

/*
 * Purpose : Determine whether the given MySQL query can be explained.
 *
 * Params  : 1. The query to test.
 *           2. The length of the query.
 *
 * Returns : Non-zero if the query is explainable; zero otherwise.
 */
extern int nr_php_explain_mysql_query_is_explainable(const char* query,
                                                     int length);

/*
 * Purpose : Generate an explain plan for the given PDOStatement object.
 *
 * Params  : 1. The transaction.
 *           2. The PDOStatement object.
 *           3. An array of parameters that should be used when binding
 *              parameters to the EXPLAIN query, instead of inspecting the
 *              pdo_stmt_t structure. If NULL, the pdo_stmt_t structure's
 *              parameters will be used.
 *           4. The start time of the original query.
 *           5. The stop time of the original query.
 *
 * Returns : An explain plan if one was generated, or NULL otherwise.
 */
extern nr_explain_plan_t* nr_php_explain_pdo_statement(nrtxn_t* txn,
                                                       zval* stmt,
                                                       zval* parameters,
                                                       nrtime_t start,
                                                       nrtime_t stop TSRMLS_DC);

/*
 * Purpose : Ascertain if we want to generate an explain plan for a query of
 *           the given duration.
 *
 * Params  : 1. The transaction.
 *           2. The duration of the original query.
 *
 * Returns : Non-zero if an explain plan is wanted; zero otherwise.
 */
extern int nr_php_explain_wanted(const nrtxn_t* txn,
                                 nrtime_t duration TSRMLS_DC);

#endif /* PHP_EXPLAIN_HDR */
