/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_MYSQLI_PRIVATE_HDR
#define PHP_MYSQLI_PRIVATE_HDR

#include "nr_datastore_instance.h"

/*
 * Purpose : Retrieve the default socket for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A pointer to a Zend-owned string.
 */
static inline char* nr_php_mysqli_default_socket() {
  return nr_php_zend_ini_string(NR_PSTR("mysqli.default_socket"), 0);
}
/*
 * Purpose : Retrieve the default port for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A pointer to a Zend-owned string.
 */
static inline char* nr_php_mysqli_default_port() {
  return nr_php_zend_ini_string(NR_PSTR("mysqli.default_port"), 0);
}
/*
 * Purpose : Retrieve the default host for a MySQL connection
 *           made by the mysql extension.
 *
 * Returns : A pointer to a Zend-owned string.
 */
extern char* nr_php_mysqli_default_host();

/*
 * Purpose : Determine the host and port_path_or_id from the parameters
 *           provided to the mysqli extension.
 *
 * Params  : 1. The host
 *           2. The port
 *           3. The socket
 *           4. A return value for the host
 *           5. A return value for the port_path_or_id
 *
 * Note    : It's the responsibility of the caller to free the returned strings
 *           after use.
 */
extern void nr_php_mysqli_get_host_and_port_path_or_id(const char* host_param,
                                                       const zend_long port,
                                                       const char* socket,
                                                       char** host,
                                                       char** port_path_or_id);

/*
 * Purpose : Create datastore instance metadata for a MySQL connection via the
 *           mysqli extension.
 *
 * Params  : 1. The host
 *           2. The port
 *           3. The socket
 *           4. The database
 *
 * Returns : A pointer to the datastore instance structure.
 */
extern nr_datastore_instance_t* nr_php_mysqli_create_datastore_instance(
    const char* host,
    const zend_long port,
    const char* socket,
    const char* database);

/*
 * Purpose : Strip the "p:" prefix from a host name, if set.
 *
 * Params  : 1. The host name.
 *
 * Returns : A pointer to the start of the real host name.
 */
extern const char* nr_php_mysqli_strip_persistent_prefix(const char* host);

#endif /* PHP_MYSQLI_PRIVATE_HDR */
