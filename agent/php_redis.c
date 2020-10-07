/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_datastore.h"
#include "php_redis.h"
#include "php_redis_private.h"
#include "util_system.h"

const uint16_t nr_php_redis_default_port = 6379;

nr_datastore_instance_t* nr_php_redis_create_datastore_instance(
    const char* host_or_socket,
    zend_long port) {
  nr_datastore_instance_t* instance = NULL;

  if (NULL == host_or_socket) {
    return NULL;
  }

  /*
   * There are two possible connection types. A UNIX socket connection will be
   * made if the host_or_socket starts with '/': in that case, the port is
   * ignored. Otherwise, a TCP connection is made with the host name and port
   * number.
   */
  if (nr_php_redis_is_unix_socket(host_or_socket)) {
    instance = nr_datastore_instance_create("localhost", host_or_socket,
                                            nr_php_redis_default_database);
  } else {
    char* port_str = nr_formatf("%ld", (long)port);

    instance = nr_datastore_instance_create(host_or_socket, port_str,
                                            nr_php_redis_default_database);

    nr_free(port_str);
  }

  return instance;
}

nr_datastore_instance_t* nr_php_redis_save_datastore_instance(
    const zval* redis_conn,
    const char* host_or_socket,
    zend_long port TSRMLS_DC) {
  nr_datastore_instance_t* instance;
  char* key;

  key = nr_php_datastore_make_key(redis_conn, "redis");
  instance = nr_php_redis_create_datastore_instance(host_or_socket, port);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(key);
  return instance;
}

nr_datastore_instance_t* nr_php_redis_retrieve_datastore_instance(
    const zval* redis_conn TSRMLS_DC) {
  nr_datastore_instance_t* instance;
  char* key;

  key = nr_php_datastore_make_key(redis_conn, "redis");
  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);
  nr_free(key);

  return instance;
}

void nr_php_redis_remove_datastore_instance(const zval* redis_conn TSRMLS_DC) {
  char* key;

  key = nr_php_datastore_make_key(redis_conn, "redis");
  nr_php_datastore_instance_remove(key TSRMLS_CC);
  nr_free(key);
}
