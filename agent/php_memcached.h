/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_MEMCACHED_HDR
#define PHP_MEMCACHED_HDR

#include "nr_datastore_instance.h"
#include "php_includes.h"

/*
 * Purpose : Create a datastore instance metadata for a Memcached server.
 *
 * Params  : 1. The memcached host or socket name as given to Memcached::addServer().
 *           2. The memcached port as given as given to Memcached::addServer().
 *
 * Returns: nr_datastore_instance_t* that the caller is responsible for freeing
 */
nr_datastore_instance_t* nr_php_memcached_create_datastore_instance(
    const char* host_or_socket,
    zend_long port);

/*
 * Purpose : Create a memcached instance metric
 *
 * Params  : 1. The memcached host or socket name as given to Memcached::addServer().
 *           2. The memcached port as given as given to Memcached::addServer().
 */
extern void nr_php_memcached_create_instance_metric(
    const char* host_or_socket,
    zend_long port);


#endif
