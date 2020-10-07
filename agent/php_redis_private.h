/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_REDIS_PRIVATE_HDR
#define PHP_REDIS_PRIVATE_HDR

/*
 * Redis uses database numbers, rather than names. By default, Redis connects
 * to database 0.
 */
static const char* nr_php_redis_default_database = "0";

/*
 * Purpose : Determine whether the given Redis host or socket is a UNIX socket.
 *
 * Params  : 1. A Redis host or socket string.
 *
 * Returns : Non-zero if the string is a UNIX socket; zero if the string is a
 *           host name.
 */
static inline int nr_php_redis_is_unix_socket(const char* host_or_socket) {
  return ((NULL != host_or_socket) && ('/' == host_or_socket[0]));
}

/*
 * Purpose : Create a new Redis datastore instance.
 *
 * Params  : 1. The host or socket name.
 *           2. The port number.
 *
 * Returns : A new datastore instance, which is owned by the caller.
 */
extern nr_datastore_instance_t* nr_php_redis_create_datastore_instance(
    const char* host_or_socket,
    zend_long port);

#endif /* PHP_REDIS_PRIVATE_HDR */
