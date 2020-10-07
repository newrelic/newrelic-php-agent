/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_MYSQL_HDR
#define PHP_MYSQL_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Create and save datastore instance metadata for a MySQL connection.
 *
 * Params  : 1. The mysql connection zval
 *           2. The concatenated mysql host and port string
 */
extern void nr_php_mysql_save_datastore_instance(const zval* mysql_conn,
                                                 const char* host_and_port
                                                     TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a MySQL connection.
 *
 * Params  : 1. The mysql connection resource
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_mysql_retrieve_datastore_instance(
    const zval* mysql_conn TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a MySQL connection.
 *
 * Params  : 1. The mysql connection resource
 */
extern void nr_php_mysql_remove_datastore_instance(
    const zval* mysql_conn TSRMLS_DC);

#endif /* PHP_MYSQL_HDR */
