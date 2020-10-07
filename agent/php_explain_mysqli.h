/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions that actually generate an explain plan for
 * MySQLi queries.
 */
#ifndef PHP_EXPLAIN_MYSQLI_HDR
#define PHP_EXPLAIN_MYSQLI_HDR

#include "nr_explain.h"

/*
 * Purpose : Generate an explain plan for a MySQLi query.
 *
 * Params  : 1. The transaction.
 *           2. The mysqli object instance.
 *           3. The SQL of the query to explain.
 *           4. The length of the SQL.
 *           5. The start time of the original query.
 *           6. The stop time of the original query.
 *
 * Returns : A pointer to an explain plan, which the caller will now own, or
 *           NULL on error.
 */
extern nr_explain_plan_t* nr_php_explain_mysqli_query(const nrtxn_t* txn,
                                                      zval* link,
                                                      const char* sql,
                                                      int sql_len,
                                                      nrtime_t start,
                                                      nrtime_t stop TSRMLS_DC);

/*
 * Purpose : Generate an explain plan for a MySQLi statement.
 *
 * Params  : 1. The transaction.
 *           2. The mysqli_stmt object instance.
 *           3. The start time of the original query.
 *           4. The stop time of the original query.
 *
 * Returns : A pointer to an explain plan, which the caller will now own, or
 *           NULL on error.
 */
extern nr_explain_plan_t* nr_php_explain_mysqli_stmt(
    const nrtxn_t* txn,
    nr_php_object_handle_t handle,
    nrtime_t start,
    nrtime_t stop TSRMLS_DC);

#endif /* PHP_EXPLAIN_MYSQLI_HDR */
