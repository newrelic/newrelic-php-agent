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
 * Defined when the agent can read the structures that mysqlnd keeps for a
 * statement: mysqli is always built on mysqlnd from PHP 8.2, and before that
 * only when MYSQLI_USE_MYSQLND is defined (a mysqli built on libmysqlclient
 * has other structures and other headers). Without it the explain of a
 * statement that was prepared in an earlier transaction runs only when the
 * statement has no parameters.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO     \
    && (ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO \
        || defined(MYSQLI_USE_MYSQLND))
#define NR_PHP_MYSQLI_MYSQLND 1
#endif

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
 *           3. Optional params to be bound during execute
 *           4. The start time of the original query.
 *           5. The stop time of the original query.
 *
 * Returns : A pointer to an explain plan, which the caller will now own, or
 *           NULL on error.
 *
 * Notes   : The link and the SQL are taken from the transaction's entry for
 *           the statement when it has both. Otherwise (PHP 8.0 and later) they
 *           are the ones saved with the statement object, which survive the
 *           transaction that prepared it. Parameters bound in an earlier
 *           transaction are read from the statement object, when mysqli is
 *           built on mysqlnd and the parameter count matches. Otherwise such
 *           a statement is not explained unless the parameters are given to
 *           execute() or bound again.
 */
extern nr_explain_plan_t* nr_php_explain_mysqli_stmt(
    const nrtxn_t* txn,
    const zval* stmt,
    nr_php_object_handle_t handle,
    zval* params,
    nrtime_t start,
    nrtime_t stop TSRMLS_DC);

#ifdef NR_PHP_MYSQLI_MYSQLND
/*
 * Purpose : Tell whether the loaded mysqli extension is built on mysqlnd,
 *           from the module's dependencies at run time.
 *
 * Returns : 1 if it is, 0 if it is not or mysqli is not loaded.
 */
extern int nr_php_explain_mysqli_uses_mysqlnd(void);

/*
 * Purpose : The number of variables that bind_param() bound on a statement.
 *
 * Params  : 1. The mysqli_stmt object.
 *
 * Returns : The count, or 0 if the zval is not a statement, nothing is bound,
 *           or the data cannot be read (mysqli is not built on mysqlnd, or the
 *           statement was not initialized).
 */
extern long nr_php_explain_mysqli_bound_count(const zval* stmt);

/*
 * Purpose : The type letter for bind_param() that stands for the type that
 *           mysqlnd holds for a bound variable.
 *
 * Params  : 1. The mysqlnd type of the bound variable (MYSQL_TYPE_*).
 *
 * Returns : 'd' for a double, 'i' for an integer, 's' for every other type.
 */
extern char nr_php_explain_mysqli_bind_type_letter(unsigned char type);
#endif

#endif /* PHP_EXPLAIN_MYSQLI_HDR */
