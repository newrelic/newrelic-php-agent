/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_MYSQL_PRIVATE_HDR
#define PHP_MYSQL_PRIVATE_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Retrieve the default port for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A newly allocated string. It's the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_php_mysql_default_port();

/*
 * Purpose : Retrieve the default host for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A newly allocated string. It's the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_php_mysql_default_host();

/*
 * Purpose : Retrieve the default socket for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A newly allocated string. It's the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_php_mysql_default_socket();

/*
 * Purpose : Determine the host and port_path_or_id from the host string
 *           provided to the mysql extension.
 *
 * Params  : 1. The host information provided to mysql
 *           2. A return value for the host
 *           3. A return value for the port_path_or_id
 *
 * Note    : It's the responsibility of the caller to free the returned strings
 *           after use.
 */
extern void nr_php_mysql_get_host_and_port_path_or_id(
    const char* host_and_port,
    char** host_ptr,
    char** port_path_or_id_ptr);

/*
 * Purpose : Create datastore instance metadata for a MySQL connection.
 *
 * Params  : 1. The host information provided to mysql
 *
 * Returns : A pointer to the datastore instance structure.
 */
extern nr_datastore_instance_t* nr_php_mysql_create_datastore_instance(
    const char* host_and_port);

#endif /* PHP_MYSQL_PRIVATE_HDR */
