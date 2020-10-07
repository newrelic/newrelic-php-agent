/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "lib_mongodb_private.h"
#include "util_system.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

/*
 * The mongodb extension requires PHP 5.4.
 */
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO

static char* system_host_name;

static void declare_server_class(TSRMLS_D) {
  tlib_php_request_eval(
      "namespace MongoDB\\Driver;"
      "class Server {"
      "protected $host;"
      "protected $port;"
      "public function __construct($host, $port) {"
      "$this->host = $host;"
      "$this->port = $port;"
      "}"
      "public function getHost() { return $this->host; }"
      "public function getPort() { return $this->port; }"
      "}" TSRMLS_CC);
}

static void test_host_and_port_path_or_id(const char* input,
                                          const char* expected_host,
                                          const char* expected_port_path_or_id
                                              TSRMLS_DC) {
  char* params = nr_formatf("new \\MongoDB\\Driver\\Server(%s)", input);
  char* host = NULL;
  char* port_path_or_id = NULL;
  zval* obj = tlib_php_request_eval_expr(params TSRMLS_CC);

  nr_mongodb_get_host_and_port_path_or_id(obj, &host,
                                          &port_path_or_id TSRMLS_CC);

  tlib_pass_if_str_equal("correct host", expected_host, host);
  tlib_pass_if_str_equal("correct port", expected_port_path_or_id,
                         port_path_or_id);

  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(params);
  nr_php_zval_free(&obj);
}

static void test_get_host_and_port_path_or_id(TSRMLS_D) {
  zval* obj;
  char* host = NULL;
  char* port_path_or_id = NULL;

  tlib_php_request_start();

  declare_server_class(TSRMLS_C);

  /*
   * Test : Bad input.
   */
  nr_mongodb_get_host_and_port_path_or_id(NULL, &host,
                                          &port_path_or_id TSRMLS_CC);
  tlib_pass_if_null("NULL server doesn't affect host", host);
  tlib_pass_if_null("NULL server doesn't affect port", port_path_or_id);

  host = nr_strdup("foo");
  port_path_or_id = nr_strdup("bar");
  nr_mongodb_get_host_and_port_path_or_id(NULL, &host,
                                          &port_path_or_id TSRMLS_CC);
  tlib_pass_if_str_equal("non-NULL host unaffected", host, "foo");
  tlib_pass_if_str_equal("non-NULL port unaffected", port_path_or_id, "bar");
  nr_free(host);
  nr_free(port_path_or_id);

  obj = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);
  nr_mongodb_get_host_and_port_path_or_id(obj, &host,
                                          &port_path_or_id TSRMLS_CC);
  tlib_pass_if_null("invalid server doesn't affect host", host);
  tlib_pass_if_null("invalid server doesn't affect port", port_path_or_id);
  nr_php_zval_free(&obj);

  test_host_and_port_path_or_id("null, null", "unknown", "unknown" TSRMLS_CC);
  test_host_and_port_path_or_id("7, 'foo'", "unknown", "unknown" TSRMLS_CC);

  /*
   * Test : Normal operation.
   *
   * This method delegates to the host and port helpers so we really just need
   * to test normal behavior and that we properly switch host and port when
   * sockets are used.
   */
  test_host_and_port_path_or_id("'localhost', 27017", system_host_name,
                                "27017" TSRMLS_CC);
  test_host_and_port_path_or_id("'my_db', 4321", "my_db", "4321" TSRMLS_CC);
  test_host_and_port_path_or_id("'/tmp/mongodb-27017.sock', 27017",
                                system_host_name,
                                "/tmp/mongodb-27017.sock" TSRMLS_CC);
  test_host_and_port_path_or_id("'/', 27017", system_host_name, "/" TSRMLS_CC);

  tlib_php_request_end();
}

static void test_host_and_port_path_or_id_individually(
    const char* input,
    const char* expected_host,
    const char* expected_port_path_or_id TSRMLS_DC) {
  char* host;
  char* port;
  char* params = nr_formatf("new \\MongoDB\\Driver\\Server(%s)", input);
  zval* obj = tlib_php_request_eval_expr(params TSRMLS_CC);

  host = nr_mongodb_get_host(obj TSRMLS_CC);
  port = nr_mongodb_get_port(obj TSRMLS_CC);

  tlib_pass_if_str_equal("correct host", expected_host, host);
  tlib_pass_if_str_equal("correct port", expected_port_path_or_id, port);

  nr_free(host);
  nr_free(port);
  nr_free(params);
  nr_php_zval_free(&obj);
}

static void test_get_host_and_port_path_or_id_individually(TSRMLS_D) {
  zval* obj;
  char* host = NULL;
  char* port_path_or_id = NULL;
  tlib_php_request_start();

  declare_server_class(TSRMLS_C);

  /*
   * Test: Bad input.
   */
  host = nr_mongodb_get_host(NULL TSRMLS_CC);
  port_path_or_id = nr_mongodb_get_port(NULL TSRMLS_CC);
  tlib_pass_if_null("NULL server returns NULL", host);
  tlib_pass_if_null("NULL server returns NULL", port_path_or_id);
  nr_free(host);
  nr_free(port_path_or_id);

  obj = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);
  host = nr_mongodb_get_host(obj TSRMLS_CC);
  port_path_or_id = nr_mongodb_get_port(obj TSRMLS_CC);
  tlib_pass_if_null("invalid server doesn't affect host", host);
  tlib_pass_if_null("invalid server doesn't affect port", port_path_or_id);
  nr_php_zval_free(&obj);

  test_host_and_port_path_or_id_individually("null, null", "unknown",
                                             "unknown" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("'', ''", "unknown",
                                             "unknown" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("7, 'foo'", "unknown",
                                             "unknown" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("0.31, 0.45", "unknown",
                                             "unknown" TSRMLS_CC);

  /*
   * Test: Normal operation.
   */
  test_host_and_port_path_or_id_individually(
      "'localhost', 27017", system_host_name, "27017" TSRMLS_CC);
  test_host_and_port_path_or_id_individually(
      "'127.0.0.1', 3311", system_host_name, "3311" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("'my_db', 27017", "my_db",
                                             "27017" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("'12', 0", "12", "0" TSRMLS_CC);

  /*
   * Test: Socket to me!
   *
   * Note that the sockets show up as host name here but they're properly
   * swapped into paths in the main method.
   */
  test_host_and_port_path_or_id_individually("'/tmp/mongodb-27017.sock', 27017",
                                             "/tmp/mongodb-27017.sock",
                                             "27017" TSRMLS_CC);
  test_host_and_port_path_or_id_individually("'/', 4444", "/",
                                             "4444" TSRMLS_CC);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();

  tlib_php_engine_create("" PTSRMLS_CC);

  test_get_host_and_port_path_or_id(TSRMLS_C);
  test_get_host_and_port_path_or_id_individually(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);

  nr_free(system_host_name);
}

#else

void test_main(void* p NRUNUSED) {}

#endif /* PHP >= 5.4 */
