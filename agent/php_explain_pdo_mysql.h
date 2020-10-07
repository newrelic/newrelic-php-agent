/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles PDO MySQL explain plans.
 */
#ifndef PHP_EXPLAIN_PDO_MYSQL_HDR
#define PHP_EXPLAIN_PDO_MYSQL_HDR

#include "nr_explain.h"

/*
 * Purpose : Returns an explain plan for the given prepared statement.
 *
 * Params  : 1. A PDOStatement object that has been executed.
 *           2. An array of parameters to bind, or NULL to use those previously
 *              bound to the PDOStatement object.
 *
 * Returns : An explain plan, or NULL if no explain plan can be generated.
 */
extern nr_explain_plan_t* nr_php_explain_pdo_mysql_statement(zval* stmt,
                                                             zval* parameters
                                                                 TSRMLS_DC);

#endif /* PHP_EXPLAIN_PDO_MYSQL_HDR */
