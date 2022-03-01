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

#endif /* PHP_PGSQL_HDR */
