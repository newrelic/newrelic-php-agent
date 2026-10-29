/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_OCI_HDR
#define PHP_OCI_HDR

#include "php_agent.h"
#include "util_strings.h"

/*
 * An oci8 statement is a PHP resource, not an object, so it cannot be tracked
 * with the weak table that is used for objects. The ids of resources are never
 * reused in a request (they come from the next free element of the resource
 * list), so the SQL is kept by id and the entries of freed statements are
 * dropped when a new statement is parsed.
 */

/*
 * Purpose : Create the store that holds the SQL of each oci8 statement. Call
 *           it once per request, before any statement exists.
 */
extern void nr_php_oci_statements_create(void);

/*
 * Purpose : Destroy the store. Call it in post-deactivate.
 */
extern void nr_php_oci_statements_destroy(void);

/*
 * Purpose : Save the SQL of an oci8 statement, replacing any SQL saved for it
 *           earlier, and drop the entries of the statements that were freed.
 *
 * Params  : 1. The statement resource. Anything else is ignored.
 *           2. The SQL. NULL or empty removes the entry.
 *           3. The length of the SQL.
 */
extern void nr_php_oci_statement_sql_set(const zval* stmt,
                                         const char* sql,
                                         nr_string_len_t sql_len);

/*
 * Purpose : Get the SQL saved for an oci8 statement.
 *
 * Returns : The SQL, which stays owned by the store, or NULL if none was
 *           saved.
 */
extern const char* nr_php_oci_statement_sql_get(const zval* stmt);

/*
 * Purpose : Count the entries in the store, for tests.
 */
extern size_t nr_php_oci_statements_count(void);

#endif /* PHP_OCI_HDR */
