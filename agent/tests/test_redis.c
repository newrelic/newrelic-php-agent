/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "php_redis.h"
#include "php_redis_private.h"
#include "util_system.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static char* default_database;
static char* system_host_name;

static void test_create_datastore_instance(void) {
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL host_or_socket",
                    nr_php_redis_create_datastore_instance(NULL, 0));

  /*
   * Test : Normal operation.
   */
  assert_datastore_instance_equals_destroy(
      "UNIX socket with 0 port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_php_redis_create_datastore_instance("/tmp/redis.sock", 0));

  assert_datastore_instance_equals_destroy(
      "UNIX socket with port set",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_php_redis_create_datastore_instance("/tmp/redis.sock", 6379));

  assert_datastore_instance_equals_destroy(
      "empty host and 0 port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "unknown",
          .port_path_or_id = "0",
      }),
      nr_php_redis_create_datastore_instance("", 0));

  assert_datastore_instance_equals_destroy(
      "empty host and port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "unknown",
          .port_path_or_id = "6379",
      }),
      nr_php_redis_create_datastore_instance("", 6379));

  assert_datastore_instance_equals_destroy(
      "localhost and 0 port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "0",
      }),
      nr_php_redis_create_datastore_instance("localhost", 0));

  assert_datastore_instance_equals_destroy(
      "localhost and port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "6379",
      }),
      nr_php_redis_create_datastore_instance("localhost", 6379));

  assert_datastore_instance_equals_destroy(
      "host and 0 port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "host.name",
          .port_path_or_id = "0",
      }),
      nr_php_redis_create_datastore_instance("host.name", 0));

  assert_datastore_instance_equals_destroy(
      "host and port",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "host.name",
          .port_path_or_id = "6379",
      }),
      nr_php_redis_create_datastore_instance("host.name", 6379));
}

static void test_is_unix_socket(void) {
  tlib_pass_if_int_equal("NULL", 0, nr_php_redis_is_unix_socket(NULL));
  tlib_pass_if_int_equal("empty", 0, nr_php_redis_is_unix_socket(""));
  tlib_pass_if_int_equal("host", 0, nr_php_redis_is_unix_socket("localhost"));
  tlib_fail_if_int_equal("socket", 0, nr_php_redis_is_unix_socket("/"));
  tlib_fail_if_int_equal("socket", 0, nr_php_redis_is_unix_socket("/tmp/foo"));
}

static void test_remove_datastore_instance(TSRMLS_D) {
  zval* redis;

  tlib_php_request_start();
  redis = tlib_php_request_eval_expr("new Redis" TSRMLS_CC);
  nr_php_redis_save_datastore_instance(redis, "host.name", 6379 TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  nr_php_redis_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_size_t_equal("NULL redis_conn", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  /*
   * Test : Normal operation.
   */
  nr_php_redis_remove_datastore_instance(redis TSRMLS_CC);
  tlib_pass_if_size_t_equal("valid redis_conn", 0,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_zval_free(&redis);
  tlib_php_request_end();
}

static void test_retrieve_datastore_instance(TSRMLS_D) {
  nr_datastore_instance_t* instance;
  zval* redis;

  tlib_php_request_start();
  redis = tlib_php_request_eval_expr("new Redis" TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL redis_conn",
                    nr_php_redis_retrieve_datastore_instance(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null("unsaved redis_conn",
                    nr_php_redis_retrieve_datastore_instance(redis TSRMLS_CC));

  instance = nr_php_redis_save_datastore_instance(redis, "host.name",
                                                  6379 TSRMLS_CC);
  tlib_pass_if_ptr_equal(
      "saved redis_conn", instance,
      nr_php_redis_retrieve_datastore_instance(redis TSRMLS_CC));

  nr_php_zval_free(&redis);
  tlib_php_request_end();
}

static void test_save_datastore_instance(TSRMLS_D) {
  zval* redis;

  tlib_php_request_start();
  redis = tlib_php_request_eval_expr("new Redis" TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL host_or_socket", nr_php_redis_save_datastore_instance(
                                               redis, NULL, 6379 TSRMLS_CC));
  tlib_pass_if_size_t_equal("NULL host_or_socket", 0,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  /*
   * Test : Normal operation.
   */
  assert_datastore_instance_equals(
      "NULL instance",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "host.name",
          .port_path_or_id = "6379",
      }),
      nr_php_redis_save_datastore_instance(NULL, "host.name", 6379 TSRMLS_CC));
  tlib_pass_if_size_t_equal("NULL instance", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  assert_datastore_instance_equals(
      "new instance",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "host.name",
          .port_path_or_id = "6379",
      }),
      nr_php_redis_save_datastore_instance(redis, "host.name", 6379 TSRMLS_CC));
  tlib_pass_if_size_t_equal("new instance", 2,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  assert_datastore_instance_equals(
      "updated instance",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/foo",
      }),
      nr_php_redis_save_datastore_instance(redis, "/foo", 6379 TSRMLS_CC));
  tlib_pass_if_size_t_equal("updated instance", 2,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_zval_free(&redis);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  default_database = nr_strdup(nr_php_redis_default_database);
  system_host_name = nr_system_get_hostname();

  test_create_datastore_instance();
  test_is_unix_socket();

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("redis" TSRMLS_CC)) {
    test_remove_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_save_datastore_instance(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);

  nr_free(default_database);
  nr_free(system_host_name);
}
