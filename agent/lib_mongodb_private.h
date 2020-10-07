/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIB_MONGODB_PRIVATE_HDR
#define LIB_MONGODB_PRIVATE_HDR

/*
 * Purpose : Retrieve the host name from a MongoDB\Driver\Server object.
 *
 * Params  : 1. The server object.
 *
 * Returns : A newly allocated string, which is owned by the caller, or NULL if
 *           an error occurred.
 */
extern char* nr_mongodb_get_host(zval* server TSRMLS_DC);

/*
 * Purpose : Retrieve the port from a MongoDB\Driver\Server object.
 *
 * Params  : 1. The server object.
 *
 * Returns : A newly allocated string, which is owned by the caller, or NULL if
 *           an error occurred.
 */
extern char* nr_mongodb_get_port(zval* server TSRMLS_DC);

/*
 * Purpose : Determine the host and port_path_or_id from a MongoDB\Driver\Server
 *           object.
 *
 * Params  : 1. The server object.
 *           2. A return value for the host
 *           3. A return value for the port_path_or_id
 *
 * Note    : It's the responsibility of the caller to free the returned strings
 *           after use.
 */
extern void nr_mongodb_get_host_and_port_path_or_id(zval* server,
                                                    char** host,
                                                    char** port TSRMLS_DC);

#endif /* LIB_MONGODB_PRIVATE_HDR */
