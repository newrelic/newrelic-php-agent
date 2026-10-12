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
#include "php_objtable.h"
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
  tlib_pass_if_null("global is null at request start",
                    NRPRG_CTX(pgsql_last_conn));

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
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG_CTX(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(NULL, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and empty conn_info", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_php_pgsql_save_datastore_instance(conn, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn_info", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG_CTX(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(conn, "" TSRMLS_CC);
  assert_datastore_instance_equals(
      "empty conn_info", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  /*
   * Test: Global updated
   *
   * Saving an instance should properly update the global with that connection's
   * key.
   */
  tlib_pass_if_str_equal("global properly set", key,
                         NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_php_pgsql_save_datastore_instance(
      conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "same conn won't save new instance", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG_CTX(datastore_connections), key, nr_strlen(key));
  nr_php_pgsql_save_datastore_instance(
      conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "new conn saves new instance", expected,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

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
  tlib_pass_if_null("global is null at request start",
                    NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: Unknown non-null connection
   */
  tlib_pass_if_null("unknown non-null connection info isn't found",
                    nr_php_pgsql_retrieve_datastore_instance(conn TSRMLS_CC));
  tlib_pass_if_null(
      "an unknown non-null connection should not update the global",
      NRPRG_CTX(pgsql_last_conn));

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
  tlib_pass_if_str_equal("global properly set", key,
                         NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_hashmap_update(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
                    nr_php_pgsql_create_datastore_instance(NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
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
  tlib_pass_if_null("global is null at request start",
                    NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: Unknown connection
   */
  key = nr_php_datastore_make_key(NULL, "pgsql");

  nr_php_pgsql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown connection has no effect", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: null connection
   */
  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
                 nr_php_pgsql_create_datastore_instance(NULL));
  nr_php_pgsql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_int_equal("removing known null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global has been reset", NRPRG_CTX(pgsql_last_conn));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "pgsql");

  nr_php_pgsql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing unknown non-null connection has no effect",
                         0, nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global still null", NRPRG_CTX(pgsql_last_conn));

  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
                 nr_php_pgsql_create_datastore_instance(NULL));
  nr_php_pgsql_remove_datastore_instance(conn TSRMLS_CC);
  tlib_pass_if_int_equal("removing known non-null connection works", 0,
                         nr_php_datastore_has_conn(key TSRMLS_CC));
  tlib_pass_if_null("global properly unset", NRPRG_CTX(pgsql_last_conn));

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

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

/*
 * From PHP 8.1 a connection is an object. A stdClass stands in for it: the
 * instance is kept per object and dropped when the object is freed.
 */
static void test_object_connections(TSRMLS_D) {
  zval* first;
  zval* second;
  nr_datastore_instance_t* expected_first = &((nr_datastore_instance_t){
      .database_name = "kirk",
      .host = "spock",
      .port_path_or_id = "2345",
  });
  nr_datastore_instance_t* expected_second = &((nr_datastore_instance_t){
      .database_name = "mccoy",
      .host = "bones",
      .port_path_or_id = "5432",
  });

  tlib_php_request_start();

  tlib_php_request_eval(
      "$first = new stdClass; $second = new stdClass;" TSRMLS_CC);
  first = tlib_php_request_eval_expr("$first" TSRMLS_CC);
  second = tlib_php_request_eval_expr("$second" TSRMLS_CC);

  tlib_pass_if_not_null("the table exists", NRPRG_SHARED(pgsql_instances));
  tlib_pass_if_null("unknown connection",
                    nr_php_pgsql_retrieve_datastore_instance(first TSRMLS_CC));

  nr_php_pgsql_save_datastore_instance(
      first, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "first connection", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(first TSRMLS_CC));
  assert_datastore_instance_equals(
      "a call with no connection uses the last", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  /* A save for a connection that has an entry replaces it. */
  nr_php_pgsql_save_datastore_instance(
      first, "host=bones port=5432 dbname=mccoy" TSRMLS_CC);
  assert_datastore_instance_equals(
      "a new connect replaces the entry", expected_second,
      nr_php_pgsql_retrieve_datastore_instance(first TSRMLS_CC));

  nr_php_pgsql_save_datastore_instance(
      second, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  assert_datastore_instance_equals(
      "second connection", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(second TSRMLS_CC));
  assert_datastore_instance_equals(
      "the first connection is untouched", expected_second,
      nr_php_pgsql_retrieve_datastore_instance(first TSRMLS_CC));
  assert_datastore_instance_equals(
      "the last connection is the second", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  /* A failed connect returns false: nothing is saved and the last is kept. */
  {
    zval failed;

    ZVAL_FALSE(&failed);
    nr_php_pgsql_save_datastore_instance(&failed, "host=x" TSRMLS_CC);
  }
  assert_datastore_instance_equals(
      "a failed connect keeps the last", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));
  tlib_pass_if_size_t_equal(
      "a failed connect saves nothing", 2,
      nr_php_objtable_count(NRPRG_SHARED(pgsql_instances)));

  /* pg_close() of another connection leaves the last alone. */
  nr_php_pgsql_remove_datastore_instance(first TSRMLS_CC);
  tlib_pass_if_null("closed connection",
                    nr_php_pgsql_retrieve_datastore_instance(first TSRMLS_CC));
  assert_datastore_instance_equals(
      "closing another connection keeps the last", expected_first,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  /* pg_close() with no connection closes the last one. */
  nr_php_pgsql_remove_datastore_instance(NULL TSRMLS_CC);
  tlib_pass_if_null("closed the last connection",
                    nr_php_pgsql_retrieve_datastore_instance(second TSRMLS_CC));
  tlib_pass_if_size_t_equal(
      "the table is empty", 0,
      nr_php_objtable_count(NRPRG_SHARED(pgsql_instances)));

  /* A connection that is freed takes its instance with it. */
  nr_php_pgsql_save_datastore_instance(
      second, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  nr_php_zval_free(&second);
  nr_php_zval_free(&first);
  tlib_php_request_eval("unset($first, $second);" TSRMLS_CC);
  tlib_pass_if_size_t_equal(
      "freed with the object", 0,
      nr_php_objtable_count(NRPRG_SHARED(pgsql_instances)));
  assert_datastore_instance_equals(
      "no last connection falls back to the default instance",
      &((nr_datastore_instance_t){
          .database_name = DEFAULT_DATABASE_NAME,
          .host = system_host_name,
          .port_path_or_id = DEFAULT_SOCKET,
      }),
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));

  tlib_php_request_end();
}

/*
 * The table is shared by all Fibers: it is the same table in every Fiber and
 * in the main code, its entries survive a Fiber suspending and resuming, and
 * the default connection is the one connected last whichever Fiber made it.
 */
static void test_fiber_shared_table(TSRMLS_D) {
  nr_php_objtable_t* table;
  zval* main_conn;
  zval* fiber_conn;
  nr_datastore_instance_t* expected_main = &((nr_datastore_instance_t){
      .database_name = "kirk",
      .host = "spock",
      .port_path_or_id = "2345",
  });
  nr_datastore_instance_t* expected_fiber = &((nr_datastore_instance_t){
      .database_name = "mccoy",
      .host = "bones",
      .port_path_or_id = "5432",
  });

  tlib_php_request_start();
  table = NRPRG_SHARED(pgsql_instances);
  tlib_pass_if_not_null("the table exists", table);

  tlib_php_request_eval(
      "$main_conn = new stdClass;"
      "$fiber = new Fiber(function () {"
      "  $fiber_conn = new stdClass;"
      "  Fiber::suspend($fiber_conn);"
      "  return 1;"
      "});"
      "$fiber_conn = $fiber->start();" TSRMLS_CC);
  main_conn = tlib_php_request_eval_expr("$main_conn" TSRMLS_CC);
  fiber_conn = tlib_php_request_eval_expr("$fiber_conn" TSRMLS_CC);

  nr_php_pgsql_save_datastore_instance(
      main_conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  nr_php_pgsql_save_datastore_instance(
      fiber_conn, "host=bones port=5432 dbname=mccoy" TSRMLS_CC);

  /* The Fiber is suspended. Resume it and let it finish. */
  tlib_php_request_eval("$fiber->resume();" TSRMLS_CC);
  tlib_pass_if_ptr_equal("same table after the Fiber ran", table,
                         NRPRG_SHARED(pgsql_instances));
  assert_datastore_instance_equals(
      "main connection after the Fiber ran", expected_main,
      nr_php_pgsql_retrieve_datastore_instance(main_conn TSRMLS_CC));
  assert_datastore_instance_equals(
      "Fiber connection after the Fiber ran", expected_fiber,
      nr_php_pgsql_retrieve_datastore_instance(fiber_conn TSRMLS_CC));
  assert_datastore_instance_equals(
      "the last connection is the one made in the Fiber", expected_fiber,
      nr_php_pgsql_retrieve_datastore_instance(NULL TSRMLS_CC));
  tlib_pass_if_size_t_equal("two entries", 2, nr_php_objtable_count(table));

  nr_php_zval_free(&main_conn);
  nr_php_zval_free(&fiber_conn);
  tlib_php_request_eval("unset($main_conn, $fiber_conn, $fiber);" TSRMLS_CC);
  tlib_pass_if_size_t_equal("freed with the objects", 0,
                            nr_php_objtable_count(table));

  tlib_php_request_end();
}

/* A connection made in a Fiber that is destroyed unfinished goes with it. */
static void test_fiber_destroyed_unfinished(TSRMLS_D) {
  zval* conn;

  tlib_php_request_start();

  tlib_php_request_eval(
      "$fiber = new Fiber(function () {"
      "  $conn = new stdClass;"
      "  Fiber::suspend($conn);"
      "});"
      "$conn = $fiber->start();" TSRMLS_CC);
  conn = tlib_php_request_eval_expr("$conn" TSRMLS_CC);
  nr_php_pgsql_save_datastore_instance(
      conn, "host=spock port=2345 dbname=kirk" TSRMLS_CC);
  nr_php_zval_free(&conn);

  tlib_php_request_eval("unset($conn);" TSRMLS_CC);
  tlib_pass_if_size_t_equal(
      "kept while the Fiber holds it", 1,
      nr_php_objtable_count(NRPRG_SHARED(pgsql_instances)));
  tlib_php_request_eval("unset($fiber);" TSRMLS_CC);
  tlib_pass_if_size_t_equal(
      "removed with the Fiber", 0,
      nr_php_objtable_count(NRPRG_SHARED(pgsql_instances)));

  tlib_php_request_end();
}

#endif

void test_main(void* p NRUNUSED) {
  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("" PTSRMLS_CC);

  if (nr_php_extension_loaded("pgsql")) {
    test_save_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_remove_datastore_instance(TSRMLS_C);
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
    test_object_connections(TSRMLS_C);
    test_fiber_shared_table(TSRMLS_C);
    test_fiber_destroyed_unfinished(TSRMLS_C);
#endif
    test_create_datastore_instance();
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
