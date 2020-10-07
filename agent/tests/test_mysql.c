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
#include "php_mysql.h"
#include "php_mysql_private.h"
#include "util_system.h"

static char* system_host_name;

#define DEFAULT_DATABASE_NAME "unknown"
#define DEFAULT_PORT "3306"
#define DEFAULT_SOCKET "mysql.sock"

static void test_save_datastore_instance(TSRMLS_D) {
  char* key = NULL;
  zval* conn;
  nr_datastore_instance_t* expected_default = &((nr_datastore_instance_t){
      .database_name = DEFAULT_DATABASE_NAME,
      .host = system_host_name,
      .port_path_or_id = DEFAULT_SOCKET,
  });
  nr_datastore_instance_t* expected = &((nr_datastore_instance_t){
      .database_name = DEFAULT_DATABASE_NAME,
      .host = "blue",
      .port_path_or_id = "3333",
  });

  tlib_php_request_start();
  conn = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  /*
   * Test: Global initialized
   */
  tlib_pass_if_null("global is null at request start", NRPRG(mysql_last_conn));

  /*
   * Test: Bad input saves the default instance information
   *
   * Note that without deleting the previous instance, the hashmap is not
   * updated with new information.
   */
  key = nr_php_datastore_make_key(NULL, "mysql");

  nr_php_mysql_save_datastore_instance(NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and null host_and_port", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_mysql_save_datastore_instance(NULL, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and empty host_and_port", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysql");

  nr_php_mysql_save_datastore_instance(conn, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null host_and_port", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_mysql_save_datastore_instance(conn, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "empty host_and_port", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  /*
   * Test: Global updated
   *
   * Saving an instance should properly update the global with that connection's
   * key.
   */
  tlib_pass_if_str_equal("global properly set", key, NRPRG(mysql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_php_mysql_save_datastore_instance(conn, "blue:3333" TSRMLS_CC);
  assert_datastore_instance_equals(
      "same conn won't save new instance", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_mysql_save_datastore_instance(conn, "blue:3333" TSRMLS_CC);
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
  tlib_pass_if_null("global is null at request start", NRPRG(mysql_last_conn));

  /*
   * Test: Unknown non-null connection
   */
  tlib_pass_if_null("unknown non-null connection info isn't found",
                    nr_php_mysql_retrieve_datastore_instance(conn TSRMLS_CC));
  tlib_pass_if_null(
      "an unknown non-null connection should not update the global",
      NRPRG(mysql_last_conn));

  /*
   * Test: Unknown null connection
   *
   * Retrieving information for an unknown null connection will create and save
   * a new default instance, updating the global.
   */
  assert_datastore_instance_equals(
      "unknown null connection saves a default instance", expected,
      nr_php_mysql_retrieve_datastore_instance(NULL TSRMLS_CC));
  key = nr_php_datastore_make_key(NULL, "mysql");
  tlib_pass_if_str_equal("global properly set", key, NRPRG(mysql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_hashmap_update(NRPRG(datastore_connections), key, nr_strlen(key),
                    nr_php_mysql_create_datastore_instance(NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_mysql_retrieve_datastore_instance(NULL TSRMLS_CC));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysql");

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysql_create_datastore_instance(NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_mysql_retrieve_datastore_instance(conn TSRMLS_CC));

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
  tlib_pass_if_null("global is null at request start", NRPRG(mysql_last_conn));

  /*
   * Test: Unknown connection
   */
  key = nr_php_datastore_make_key(NULL, "mysql");

  nr_php_mysql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown connection has no effect", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG(mysql_last_conn));

  /*
   * Test: null connection
   */
  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysql_create_datastore_instance(NULL));
  nr_php_mysql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing known null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global has been reset", NRPRG(mysql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysql");

  nr_php_mysql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown non-null connection has no effect",
                         0, nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG(mysql_last_conn));

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysql_create_datastore_instance(NULL));
  nr_php_mysql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing known non-null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global properly unset", NRPRG(mysql_last_conn));

  nr_free(key);
  nr_php_zval_free(&conn);

  tlib_php_request_end();
}

static void test_default_port_host_and_socket() {
  char* port = NULL;
  char* host = NULL;
  char* socket = NULL;

  /*
   * Test: Normal operation
   */
  port = nr_php_mysql_default_port();
  tlib_pass_if_str_equal("default port", DEFAULT_PORT, port);

  host = nr_php_mysql_default_host();
  tlib_pass_if_str_equal("default host", "localhost", host);

  socket = nr_php_mysql_default_socket();
  tlib_pass_if_str_equal("default socket", DEFAULT_SOCKET, socket);

  nr_free(port);
  nr_free(host);
  nr_free(socket);
}

static void test_host_and_port_path_or_id_early_return(void) {
  char* host = nr_strdup("no");
  char* port_path_or_id = nr_strdup("nope");

  /*
   * Test: Non-NULL return value params don't blow up
   */
  nr_php_mysql_get_host_and_port_path_or_id("", &host, &port_path_or_id);

  nr_free(host);
  nr_free(port_path_or_id);
}

static void test_host_and_port_path_or_id(
    const char* host_and_port,
    const char* expected_host,
    const char* expected_port_path_or_id) {
  char* host = NULL;
  char* port_path_or_id = NULL;

  nr_php_mysql_get_host_and_port_path_or_id(host_and_port, &host,
                                            &port_path_or_id);
  tlib_pass_if_str_equal("correct host", expected_host, host);
  tlib_pass_if_str_equal("correct port_path_or_id", expected_port_path_or_id,
                         port_path_or_id);

  nr_free(host);
  nr_free(port_path_or_id);
}

static void test_get_host_and_port_path_or_id() {
  /*
   * Test: Bad input
   */
  test_host_and_port_path_or_id_early_return();
  test_host_and_port_path_or_id("", "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id(NULL, "localhost", DEFAULT_SOCKET);

  /*
   * Test: Localhost
   */
  test_host_and_port_path_or_id("localhost", "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost:1234", "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost:", "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id(":", "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost:/path/to/socket", "localhost",
                                "/path/to/socket");
  test_host_and_port_path_or_id(":/path/to/socket", "localhost",
                                "/path/to/socket");

  /*
   * Test: Non-localhost
   */
  test_host_and_port_path_or_id("blue:star", "blue", "star");
  test_host_and_port_path_or_id("blue:/path/to/socket", "blue",
                                "/path/to/socket");
  test_host_and_port_path_or_id("blue:", "blue", DEFAULT_PORT);
  test_host_and_port_path_or_id("blue", "blue", DEFAULT_PORT);
  test_host_and_port_path_or_id("12:41", "12", "41");
}

static void test_instance(const char* message,
                          const char* host_and_port,
                          const nr_datastore_instance_t* expected) {
  nr_datastore_instance_t* actual;

  actual = nr_php_mysql_create_datastore_instance(host_and_port);
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
  test_instance("localhost", "localhost",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  /*
   * Test: Non-localhost
   */
  test_instance("non-localhost", "blue:1234",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = "blue",
                    .port_path_or_id = "1234",
                }));
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("mysql.default_socket=" DEFAULT_SOCKET PTSRMLS_CC);

  if (tlib_php_require_extension("mysql" TSRMLS_CC)) {
    test_save_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_remove_datastore_instance(TSRMLS_C);
    test_default_port_host_and_socket();
    test_get_host_and_port_path_or_id();
    test_create_datastore_instance();
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
