/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_datastore.h"
#include "tlib_main.h"
#include "tlib_php.h"

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_datastore.h"
#include "php_pgsql.h"
#include "php_pgsql_private.h"
#include "util_system.h"

static char* system_host_name;

#define DEFAULT_DATABASE_NAME "unknown"
#define DEFAULT_PORT "5432"
#define DEFAULT_SOCKET "/tmp"

static void test_save_datastore_instance(TSRMLS_D) {
  char* key = NULL;
  zval* conn;
  nr_datastore_instance_t* expected_default = &((nr_datastore_instance_t){
      .database_name = DEFAULT_DATABASE_NAME,
      .host = system_host_name,
      .port_path_or_id = DEFAULT_SOCKET,
  });
  nr_datastore_instance_t* expected = &((nr_datastore_instance_t){
      .database_name = "kirk",
      .host = "spock",
      .port_path_or_id = "2345",
  });

  tlib_php_request_start();
  conn = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  /*
   * Test: Global initialized
   */
  tlib_pass_if_null("global is null at request start", NRPRG(pgsql_last_conn));

  /*
   * Test: Bad input saves the default instance information
   *
   * Note that without deleting the previous instance, the hashmap is not
   * updated with new information.
   */
  key = nr_php_datastore_make_key(NULL, "pgsql");

  nr_php_pgsql_save_datastore_instance(NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and null conn_info", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(NULL, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and empty conn_info", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_php_pgsql_save_datastore_instance(conn, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn_info", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(conn, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "empty conn_info", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  /*
   * Test: Global updated
   *
   * Saving an instance should properly update the global with that connection's
   * key.
   */
  tlib_pass_if_str_equal("global properly set", key, NRPRG(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_php_pgsql_save_datastore_instance(
      conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "same conn won't save new instance", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(
      conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "new conn saves new instance", expected,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_php_zval_free(&conn);
  nr_free(key);

  tlib_php_request_end();
}

static void test_retrieve_datastore_instance(TSRMLS_D) {
  char* key = NULL;
  zval* conn;
  nr_datastore_instance_t* expected = &((nr_datastore_instance_t){
      .database_name = DEFAULT_DATABASE_NAME,
      .host = system_host_name,
      .port_path_or_id = DEFAULT_SOCKET,
  });

  tlib_php_request_start();
  conn = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  /*
   * Test: Global initialized
   */
  tlib_pass_if_null("global is null at request start", NRPRG(pgsql_last_conn));

  /*
   * Test: Unknown non-null connection
   */
  tlib_pass_if_null("unknown non-null connection info isn't found",
                    nr_php_pgsql_retrieve_datastore_instance(conn TSRMLS_CC));
  tlib_pass_if_null(
      "an unknown non-null connection should not update the global",
      NRPRG(pgsql_last_conn));

  /*
   * Test: Unknown null connection
   *
   * Retrieving information for an unknown null connection will create and save
   * a new default instance, updating the global.
   */
  assert_datastore_instance_equals(
      "unknown null connection saves a default instance", expected,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));
  key = nr_php_datastore_make_key(NULL, "pgsql");
  tlib_pass_if_str_equal("global properly set", key, NRPRG(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_hashmap_update(NRPRG(datastore_connections), key, nr_strlen(key),
                    nr_php_pgsql_create_datastore_instance(NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_pgsql_create_datastore_instance(NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_pgsql_retrieve_datastore_instance(conn TSRMLS_CC));

  nr_php_zval_free(&conn);
  nr_free(key);

  tlib_php_request_end();
}

static void test_remove_datastore_instance(TSRMLS_D) {
  zval* conn;
  char* key = NULL;

  tlib_php_request_start();
  conn = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  /*
   * Test: Global initialized
   */
  tlib_pass_if_null("global is null at request start", NRPRG(pgsql_last_conn));

  /*
   * Test: Unknown connection
   */
  key = nr_php_datastore_make_key(NULL, "pgsql");

  nr_php_pgsql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown connection has no effect", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG(pgsql_last_conn));

  /*
   * Test: null connection
   */
  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_pgsql_create_datastore_instance(NULL));
  nr_php_pgsql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing known null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global has been reset", NRPRG(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_php_pgsql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown non-null connection has no effect",
                         0, nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG(pgsql_last_conn));

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_pgsql_create_datastore_instance(NULL));
  nr_php_pgsql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing known non-null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global properly unset", NRPRG(pgsql_last_conn));

  nr_free(key);
  nr_php_zval_free(&conn);

  tlib_php_request_end();
}

static void test_instance(const char* message,
                          const char* conn_info,
                          const nr_datastore_instance_t* expected) {
  nr_datastore_instance_t* actual;

  actual = nr_php_pgsql_create_datastore_instance(conn_info);
  assert_datastore_instance_equals(message, expected, actual);

  nr_datastore_instance_destroy(&actual);
}

static void test_create_datastore_instance() {
  /*
   * Test: Bad input
   */
  test_instance("null", NULL,
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  test_instance("empty", "",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  /*
   * Test: localhost
   */
  test_instance("localhost port", "host=localhost",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_PORT,
                }));

  test_instance("localhost socket", "host=/tmp",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  /*
   * Test: Non-localhost
   */
  test_instance("non-localhost", "host=spock port=2345 dbname=kirk",
                &((nr_datastore_instance_t){
                    .database_name = "kirk",
                    .host = "spock",
                    .port_path_or_id = "2345",
                }));
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("" PTSRMLS_CC);

  if (nr_php_extension_loaded("pgsql")) {
    test_save_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_remove_datastore_instance(TSRMLS_C);
    test_create_datastore_instance();
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
