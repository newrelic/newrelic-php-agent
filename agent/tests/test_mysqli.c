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
#include "php_mysqli.h"
#include "php_mysqli_private.h"
#include "php_agent.h"
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
      .port_path_or_id = DEFAULT_PORT,
  });

  tlib_php_request_start();
  conn = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  /*
   * Test: Bad input saves the default instance information
   */
  key = nr_php_datastore_make_key(NULL, "mysqli");

  nr_php_mysqli_save_datastore_instance(NULL, NULL, 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and null host", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_php_mysqli_save_datastore_instance(NULL, "", 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and empty host", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysqli");

  nr_php_mysqli_save_datastore_instance(conn, NULL, 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null host saves default instance", expected_default,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_php_mysqli_save_datastore_instance(conn, "blue", 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "same conn saves new instance", expected,
      nr_hashmap_get(NRPRG(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
  nr_php_mysqli_save_datastore_instance(conn, "blue", 0, NULL, NULL TSRMLS_CC);
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
   * Test: Unknown connection
   */
  tlib_pass_if_null("unknown null connection info isn't found",
                    nr_php_mysqli_retrieve_datastore_instance(NULL TSRMLS_CC));
  tlib_pass_if_null("unknown non-null connection info isn't found",
                    nr_php_mysqli_retrieve_datastore_instance(conn TSRMLS_CC));

  /*
   * Test: Normal operation
   */
  key = nr_php_datastore_make_key(NULL, "mysqli");

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysqli_create_datastore_instance(NULL, 0, NULL, NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_mysqli_retrieve_datastore_instance(NULL TSRMLS_CC));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysqli");

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysqli_create_datastore_instance(NULL, 0, NULL, NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_mysqli_retrieve_datastore_instance(conn TSRMLS_CC));

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
   * Test: Unknown connection
   */
  key = nr_php_datastore_make_key(NULL, "mysqli");

  nr_php_mysqli_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown connection has no effect", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));

  /*
   * Test: null connection
   */
  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysqli_create_datastore_instance(NULL, 0, NULL, NULL));
  nr_php_mysqli_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing known null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysqli");

  nr_php_mysqli_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown non-null connection has no effect",
                         0, nr_php_datastore_has_conn(key TSRMLS_CC));

  nr_hashmap_set(NRPRG(datastore_connections), key, nr_strlen(key),
                 nr_php_mysqli_create_datastore_instance(NULL, 0, NULL, NULL));
  nr_php_mysqli_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing known non-null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));

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
  port = nr_php_mysqli_default_port();
  tlib_pass_if_str_equal("default port", DEFAULT_PORT, port);

  socket = nr_php_mysqli_default_socket();
  tlib_pass_if_str_equal("default socket", DEFAULT_SOCKET, socket);

  host = nr_php_mysqli_default_host();
  tlib_pass_if_str_equal("default host", "localhost", host);
}

static void test_host_and_port_path_or_id_early_return(void) {
  char* host = nr_strdup("no");
  char* port_path_or_id = nr_strdup("nope");

  /*
   * Test: Non-NULL return value params don't blow up
   */
  nr_php_mysqli_get_host_and_port_path_or_id("", 0, "", &host,
                                             &port_path_or_id);

  nr_free(host);
  nr_free(port_path_or_id);
}

static void test_host_and_port_path_or_id(
    const char* message,
    const char* host_param,
    zend_long port,
    const char* socket,
    const char* expected_host,
    const char* expected_port_path_or_id) {
  char* host = NULL;
  char* port_path_or_id = NULL;

  nr_php_mysqli_get_host_and_port_path_or_id(host_param, port, socket, &host,
                                             &port_path_or_id);
  tlib_pass_if_str_equal(message, expected_host, host);
  tlib_pass_if_str_equal(message, expected_port_path_or_id, port_path_or_id);

  nr_free(host);
  nr_free(port_path_or_id);
}

static void test_get_host_and_port_path_or_id() {
  /*
   * Test: Bad input
   */
  test_host_and_port_path_or_id_early_return();
  test_host_and_port_path_or_id("empty host", "", 0, NULL, "localhost",
                                DEFAULT_SOCKET);
  test_host_and_port_path_or_id("null host", NULL, 0, NULL, "localhost",
                                DEFAULT_SOCKET);

  /*
   * Test: Localhost
   */
  test_host_and_port_path_or_id("localhost", "localhost", 0, NULL, "localhost",
                                DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost ignores port", "localhost", 1234,
                                NULL, "localhost", DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost ignores port with empty socket",
                                "localhost", 1234, "", "localhost",
                                DEFAULT_SOCKET);
  test_host_and_port_path_or_id("localhost custom socket", "localhost", 0,
                                "/path/to/socket", "localhost",
                                "/path/to/socket");
  test_host_and_port_path_or_id("localhost custom socket ignores port",
                                "localhost", 4321, "/path/to/socket",
                                "localhost", "/path/to/socket");

  /*
   * Test: Looks like localhost, but isn't
   */
  test_host_and_port_path_or_id("colons not meaningful", "localhost:", 0, NULL,
                                "localhost:", DEFAULT_PORT);
  test_host_and_port_path_or_id("colons not meaningful",
                                "localhost:/path/to/socket", 0, NULL,
                                "localhost:/path/to/socket", DEFAULT_PORT);
  test_host_and_port_path_or_id("colons not meaningful", ":/path/to/socket", 0,
                                NULL, ":/path/to/socket", DEFAULT_PORT);

  /*
   * Test: Non-localhost
   */
  test_host_and_port_path_or_id("non-localhost socket ignored", "blue", 0,
                                "/path/to/socket", "blue", DEFAULT_PORT);
  test_host_and_port_path_or_id("non-localhost socket ignored", "blue", 42, "",
                                "blue", "42");
  test_host_and_port_path_or_id("weird host and port", "12", 41, NULL, "12",
                                "41");
}

static void test_instance(const char* message,
                          const char* host_param,
                          zend_long port,
                          const char* socket,
                          const char* database,
                          const nr_datastore_instance_t* expected) {
  nr_datastore_instance_t* actual;

  actual = nr_php_mysqli_create_datastore_instance(host_param, port, socket,
                                                   database);
  assert_datastore_instance_equals(message, expected, actual);

  nr_datastore_instance_destroy(&actual);
}

static void test_create_datastore_instance() {
  /*
   * Test: Bad input
   */
  test_instance("null", NULL, 0, NULL, NULL,
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  test_instance("empty", "", 0, "", "",
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  /*
   * Test: localhost
   */
  test_instance("localhost without db", "localhost", 0, NULL, NULL,
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  test_instance("localhost with db", "localhost", 0, NULL, "lemon_poppyseed",
                &((nr_datastore_instance_t){
                    .database_name = "lemon_poppyseed",
                    .host = system_host_name,
                    .port_path_or_id = DEFAULT_SOCKET,
                }));

  /*
   * Test: Non-localhost
   */
  test_instance("non-localhost without db", "blue", 1234, NULL, NULL,
                &((nr_datastore_instance_t){
                    .database_name = DEFAULT_DATABASE_NAME,
                    .host = "blue",
                    .port_path_or_id = "1234",
                }));

  test_instance("non-localhost with db", "blue", 1234, NULL, "lemon_poppyseed",
                &((nr_datastore_instance_t){
                    .database_name = "lemon_poppyseed",
                    .host = "blue",
                    .port_path_or_id = "1234",
                }));
}

static void test_strip_persistent_prefix(void) {
  tlib_pass_if_null("a NULL host should return NULL",
                    nr_php_mysqli_strip_persistent_prefix(NULL));

  tlib_pass_if_str_equal("an empty host should return an empty string", "",
                         nr_php_mysqli_strip_persistent_prefix(""));

  tlib_pass_if_str_equal("a single character host should return the same host",
                         "a", nr_php_mysqli_strip_persistent_prefix("a"));

  tlib_pass_if_str_equal("an unprefixed host should return the same host",
                         "host.name",
                         nr_php_mysqli_strip_persistent_prefix("host.name"));

  tlib_pass_if_str_equal(
      "a prefixed host with no name after it should return an empty string", "",
      nr_php_mysqli_strip_persistent_prefix("p:"));

  tlib_pass_if_str_equal("a prefixed host should return the unprefixed name",
                         "host.name",
                         nr_php_mysqli_strip_persistent_prefix("p:host.name"));
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("mysqli.default_socket=" DEFAULT_SOCKET PTSRMLS_CC);

  if (tlib_php_require_extension("mysqli" TSRMLS_CC)) {
    test_save_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_remove_datastore_instance(TSRMLS_C);
    test_default_port_host_and_socket();
    test_get_host_and_port_path_or_id();
    test_create_datastore_instance();
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);

  test_strip_persistent_prefix();
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
