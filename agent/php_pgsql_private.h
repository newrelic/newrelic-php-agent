/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_PGSQL_PRIVATE_HDR
#define PHP_PGSQL_PRIVATE_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Create datastore instance metadata for a Postgres connection via
 * the pgsql extension.
 *
 * Params  : 1. The connection string
 *
 * Returns : A pointer to the datastore instance structure.
 */
extern nr_datastore_instance_t* nr_php_pgsql_create_datastore_instance(
    const char* conn_info);

#endif /* PHP_PGSQL_PRIVATE_HDR */
