/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_RELAY_PRIVATE_HDR
#define PHP_RELAY_PRIVATE_HDR

/*
 * Purpose : Create a new Relay datastore instance.
 *
 * Params  : 1. The host or socket name.
 *           2. The port number.
 *
 * Returns : A new datastore instance, which is owned by the caller.
 */
extern nr_datastore_instance_t* nr_php_relay_create_datastore_instance(
    const char* host_or_socket,
    zend_long port);

#endif /* PHP_RELAY_PRIVATE_HDR */
