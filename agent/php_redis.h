/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_REDIS_HDR
#define PHP_REDIS_HDR

#include "nr_datastore_instance.h"

/*
 * The default Redis port.
 */
extern const uint16_t nr_php_redis_default_port;

/*
 * Purpose : Create and save datastore instance metadata for a Redis connection.
 *
 * Params  : 1. The Redis object.
 *           2. The Redis host or socket name as given to Redis::connect().
 *           3. The Redis port as given as given to Redis::connect().
 */
extern nr_datastore_instance_t* nr_php_redis_save_datastore_instance(
    const zval* redis_conn,
    const char* host_or_socket,
    zend_long port TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a Redis connection.
 *
 * Params  : 1. The Redis object.
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_redis_retrieve_datastore_instance(
    const zval* redis_conn TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a Redis connection.
 *
 * Params  : 1. The Redis object.
 */
extern void nr_php_redis_remove_datastore_instance(
    const zval* redis_conn TSRMLS_DC);

#endif /* PHP_REDIS_HDR */
