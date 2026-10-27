/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_PGSQL_HDR
#define PHP_PGSQL_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Create and save datastore instance metadata for a pgsql connection.
 *
 * Params  : 1. The pgsql connection resource (object in >= PHP 8.1)
 *           2. The connection string
 */
extern void nr_php_pgsql_save_datastore_instance(const zval* pgsql_conn,
                                                 const char* conn_info
                                                     TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a pgsql connection.
 *
 * Params  : 1. The pgsql connection resource (object in >= PHP 8.1)
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_pgsql_retrieve_datastore_instance(
    const zval* pgsql_conn TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a pgsql connection.
 *
 * Params  : 1. The pgsql connection resource (object in >= PHP 8.1)
 */
extern void nr_php_pgsql_remove_datastore_instance(
    const zval* pgsql_conn TSRMLS_DC);

/*
 * Purpose : Destroy the datastore instance stored for a pgsql connection
 *           object. This is the value destructor of the table in
 *           shared_globals_t.pgsql_instances.
 */
extern void nr_php_pgsql_instance_destroy(void* instance);

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
/*
 * The most statements kept for one connection. When a statement with a new
 * name is saved on a connection that has this many, the statement used least
 * recently is dropped.
 */
#define NR_PHP_PGSQL_MAX_STATEMENTS 1000

/*
 * Purpose : Destroy the statements stored for a pgsql connection object. This
 *           is the value destructor of the table in
 *           shared_globals_t.pgsql_statements.
 */
extern void nr_php_pgsql_statements_destroy(void* statements);

/*
 * Purpose : Save the SQL of a prepared statement of a connection, replacing
 *           the SQL saved under the same name.
 *
 * Params  : 1. The connection object, or NULL for the connection a call with
 *              no connection uses (the last one).
 *           2. The statement name. It can be empty.
 *           3. The length of the name.
 *           4. The SQL.
 *           5. The length of the SQL.
 */
extern void nr_php_pgsql_statement_save(const zval* pgsql_conn,
                                        const char* name,
                                        nr_string_len_t name_len,
                                        const char* sql,
                                        nr_string_len_t sql_len);

/*
 * Purpose : Get the SQL saved for a prepared statement of a connection.
 *
 * Returns : The SQL, which stays owned by the table, or NULL if none was
 *           saved.
 */
extern const char* nr_php_pgsql_statement_find(const zval* pgsql_conn,
                                               const char* name,
                                               nr_string_len_t name_len);
#endif

#endif /* PHP_PGSQL_HDR */
