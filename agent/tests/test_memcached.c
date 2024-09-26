/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "php_memcached.h"
#include "util_system.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static char* system_host_name;

static void test_create_datastore_instance(void) {
  /*
   * Test : Normal operation.
   */
  assert_datastore_instance_equals_destroy(
      "named socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "/tmp/memcached.sock",
      }),
      nr_php_memcached_create_datastore_instance("/tmp/memcached.sock", 0));

  assert_datastore_instance_equals_destroy(
      "empty socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "unknown",
      }),
      nr_php_memcached_create_datastore_instance("", 0));

  assert_datastore_instance_equals_destroy(
      "empty host",
      &((nr_datastore_instance_t){
          .host = "unknown",
          .database_name = "unknown",
          .port_path_or_id = "6379",
      }),
      nr_php_memcached_create_datastore_instance("", 6379));

  assert_datastore_instance_equals_destroy(
      "localhost socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "localhost",
      }),
      nr_php_memcached_create_datastore_instance("localhost", 0));

  assert_datastore_instance_equals_destroy(
      "localhost and port",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "6379",
      }),
      nr_php_memcached_create_datastore_instance("localhost", 6379));

  assert_datastore_instance_equals_destroy(
      "host.name socket",
      &((nr_datastore_instance_t){
          .host = system_host_name,
          .database_name = "unknown",
          .port_path_or_id = "host.name",
      }),
      nr_php_memcached_create_datastore_instance("host.name", 0));

  assert_datastore_instance_equals_destroy(
      "host and port",
      &((nr_datastore_instance_t){
          .host = "host.name",
          .database_name = "unknown",
          .port_path_or_id = "6379",
      }),
      nr_php_memcached_create_datastore_instance("host.name", 6379));
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();

  test_create_datastore_instance();

  nr_free(system_host_name);
}
