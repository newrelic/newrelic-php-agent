/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions that are useful when tracking and instrumenting
 * MySQLi connections and queries. Although created as part of the explain plan
 * project, these functions are explicitly not explain plan related: those are
 * in agent/php_explain_mysqli.[ch].
 */
#ifndef PHP_MYSQLI_HDR
#define PHP_MYSQLI_HDR

#include "php_agent.h"

/*
 * Purpose : Duplicate a MySQLi link based on the metadata in
 *           NRPRG (mysqli_links).
 *
 * Params  : 1. The mysqli object to duplicate.
 *
 * Returns : The new mysqli object, or NULL if an error occurred.
 */
extern zval* nr_php_mysqli_link_duplicate(zval* orig TSRMLS_DC);

/*
 * Purpose : Get the MySQLi link that was used to prepare a statement.
 *
 * Params  : 1. The object handle of the statement.
 *
 * Returns : A reference to the MySQLi link, or NULL on error. The reference
 *           count on the zval will not be incremented, so it should not be
 *           destroyed by the caller.
 */
extern zval* nr_php_mysqli_query_get_link(
    nr_php_object_handle_t handle TSRMLS_DC);

/*
 * Purpose : Get the SQL that was used to prepare a statement.
 *
 * Params  : 1. The object handle of the statement.
 *
 * Returns : A copy of the SQL, which the caller should free, or NULL on error.
 */
extern char* nr_php_mysqli_query_get_query(
    nr_php_object_handle_t handle TSRMLS_DC);

/*
 * Purpose : Rebind all parameters bound to a statement onto a new statement.
 *
 * Params  : 1. The object handle of the original statement.
 *           2. The mysqli_stmt instance to bind the parameters onto.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_mysqli_query_rebind(nr_php_object_handle_t handle,
                                              zval* dest TSRMLS_DC);

/*
 * Purpose : Save the parameters bound to a MySQLi statement.
 *
 * Params  : 1. The object handle of the statement.
 *           2. The format given to mysqli_stmt::bind_param().
 *           3. The length of the format.
 *           4. The number of parameters that were bound.
 *           5. An array of zval * parameters.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_mysqli_query_set_bind_params(
    nr_php_object_handle_t handle,
    const char* format,
    size_t format_len,
    size_t args_len,
    zval** args TSRMLS_DC);

/*
 * Purpose : Save the MySQLi link that prepared a statement.
 *
 * Params  : 1. The object handle of the statement.
 *           2. The MySQLi link.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_mysqli_query_set_link(
    nr_php_object_handle_t query_handle,
    zval* link TSRMLS_DC);

/*
 * Purpose : Save the SQL used to prepare a MySQLi statement.
 *
 * Params  : 1. The object handle of the statement.
 *           2. The SQL.
 *           3. The length of the SQL.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_php_mysqli_query_set_query(nr_php_object_handle_t handle,
                                                 const char* query,
                                                 int query_len TSRMLS_DC);

/*
 * Purpose : Test if the given zval is a valid mysqli object.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid mysqli object; zero otherwise.
 */
extern int nr_php_mysqli_zval_is_link(const zval* zv TSRMLS_DC);

/*
 * Purpose : Test if the given zval is a valid mysqli_stmt object.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid mysqli_stmt object; zero
 *           otherwise.
 */
extern int nr_php_mysqli_zval_is_stmt(const zval* zv TSRMLS_DC);

/*
 * Purpose : Create and save datastore instance metadata for a mysqli
 * connection.
 *
 * Params  : 1. The mysqli connection object
 *           2. The host
 *           3. The port
 *           4. The socket
 *           5. The database name
 */
extern void nr_php_mysqli_save_datastore_instance(const zval* mysqli_obj,
                                                  const char* host,
                                                  const zend_long port,
                                                  const char* socket,
                                                  const char* database
                                                      TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a mysqli connection.
 *
 * Params  : 1. The mysqli connection object
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_mysqli_retrieve_datastore_instance(
    const zval* mysqli_obj TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a mysqli connection.
 *
 * Params  : 1. The mysqli connection object
 */
extern void nr_php_mysqli_remove_datastore_instance(
    const zval* mysqli_obj TSRMLS_DC);

#endif /* PHP_MYSQLI_HDR */
