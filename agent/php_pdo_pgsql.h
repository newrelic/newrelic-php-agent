/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_PDO_PGSQL_HDR
#define PHP_PDO_PGSQL_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Create datastore instance metadata for a Postgres PDO connection.
 *
 * Params  : 1. The database connection struct from PDO.
 *
 * Returns : A pointer to a new datastore instance structure (ownership
 *           transfers to the caller), or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_pdo_pgsql_create_datastore_instance(
    pdo_dbh_t* dbh TSRMLS_DC);

#endif /* PHP_PDO_PGSQL_HDR */
