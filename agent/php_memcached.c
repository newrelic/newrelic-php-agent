/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_memcached.h"
#include "nr_datastore_instance.h"
#include "php_agent.h"

nr_datastore_instance_t* nr_php_memcached_create_datastore_instance(
    const char* host_or_socket,
    zend_long port) {
  nr_datastore_instance_t* instance = NULL;
  if (port == 0) { // local socket
    instance = nr_datastore_instance_create("localhost", host_or_socket, NULL);
  } else {
    char* port_str = nr_formatf("%ld", (long)port);
    instance  = nr_datastore_instance_create(host_or_socket, port_str, NULL);
    nr_free(port_str);
  }
  return instance;
}

