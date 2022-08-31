/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_RELAY_HDR
#define PHP_RELAY_HDR

#include "nr_datastore_instance.h"

/*
 * The default Relay port.
 */
extern const uint16_t nr_php_relay_default_port;

/*
 * Purpose : Create and save datastore instance metadata for a Relay connection.
 *
 * Params  : 1. The Relay object.
 *           2. The Relay host or socket name as given to Relay::connect().
 *           3. The Relay port as given as given to Relay::connect().
 */
extern nr_datastore_instance_t* nr_php_relay_save_datastore_instance(
    const zval* relay_conn,
    const char* host_or_socket,
    zend_long port TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a Relay connection.
 *
 * Params  : 1. The Relay object.
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_relay_retrieve_datastore_instance(
    const zval* relay_conn TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a Relay connection.
 *
 * Params  : 1. The Relay object.
 */
extern void nr_php_relay_remove_datastore_instance(
    const zval* relay_conn TSRMLS_DC);

#endif /* PHP_RELAY_HDR */
