/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"
#include "tlib_datastore.h"

#include "php_agent.h"
#include "lib_predis_private.h"
#include "util_system.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static char* default_database;
static char* default_port;
static char* system_host_name;

static void declare_parameters_class(const char* ns,
                                     const char* iface,
                                     const char* klass TSRMLS_DC) {
  char* source = nr_formatf(
      "namespace %s;"
      "interface %s { public function __get($name); }"
      "class %s implements %s {"
      "public function __construct($scheme, $host, $port, $path, $database) {"
      "$this->scheme = $scheme;"
      "$this->host = $host;"
      "$this->port = $port;"
      "$this->path = $path;"
      "$this->database = $database;"
      "}"
      "public function __get($name) { return $this->$name; }"
      "}",
      ns, iface, klass, iface);

  tlib_php_request_eval(source TSRMLS_CC);

  nr_free(source);
}

/*
 * Helper that creates the zvals for
 * nr_predis_create_datastore_instance_from_fields() from PHP literals. If the
 * literal is NULL, the zval * given to
 * nr_predis_create_datastore_instance_from_fields() will also be NULL.
 */
typedef struct {
  const char* scheme;
  const char* host;
  const char* port;
  const char* path;
  const char* database;
} field_literals_t;

static nr_datastore_instance_t* create_datastore_instance_from_fields(
    const field_literals_t* literals TSRMLS_DC) {
  nr_datastore_instance_t* instance;
  zval* scheme_zv = literals->scheme
                        ? tlib_php_request_eval_expr(literals->scheme TSRMLS_CC)
                        : NULL;
  zval* host_zv = literals->host
                      ? tlib_php_request_eval_expr(literals->host TSRMLS_CC)
                      : NULL;
  zval* port_zv = literals->port
                      ? tlib_php_request_eval_expr(literals->port TSRMLS_CC)
                      : NULL;
  zval* path_zv = literals->path
                      ? tlib_php_request_eval_expr(literals->path TSRMLS_CC)
                      : NULL;
  zval* database_zv
      = literals->database
            ? tlib_php_request_eval_expr(literals->database TSRMLS_CC)
            : NULL;

  instance = nr_predis_create_datastore_instance_from_fields(
      scheme_zv, host_zv, port_zv, path_zv, database_zv);

  nr_php_zval_free(&scheme_zv);
  nr_php_zval_free(&host_zv);
  nr_php_zval_free(&port_zv);
  nr_php_zval_free(&path_zv);
  nr_php_zval_free(&database_zv);

  return instance;
}

static void test_create_datastore_instance_from_fields(TSRMLS_D) {
  size_t i;
  zval** invalid_zvals;
  zval* scheme;

  tlib_php_request_start();

  /*
   * Test : Valid inputs, including defaults.
   */
  assert_datastore_instance_equals_destroy(
      "all defaults",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = NULL,
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "database as number",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = "1",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "database as numeric string",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = "'1'",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "database as non-numeric string",
      &((nr_datastore_instance_t){
          .database_name = "foo",
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = "'foo'",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "unix defaults",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "unknown",
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = "'unix'",
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = NULL,
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "unix all values",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = "'unix'",
          .host = "'foo.bar'",
          .port = "9999",
          .path = "'/tmp/redis.sock'",
          .database = "1",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "non-unix path only",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = "'tcp'",
          .host = NULL,
          .port = NULL,
          .path = "'/tmp/redis.sock'",
          .database = NULL,
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "non-unix all values",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = "foo.bar",
          .port_path_or_id = "9999",
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = "'tcp'",
          .host = "'foo.bar'",
          .port = "9999",
          .path = "'/tmp/redis.sock'",
          .database = "1",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "NULL scheme all values",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = "foo.bar",
          .port_path_or_id = "9999",
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = "'foo.bar'",
          .port = "9999",
          .path = "'/tmp/redis.sock'",
          .database = "1",
      }) TSRMLS_CC));

  /*
   * Test : Invalid schemes.
   */
  invalid_zvals = tlib_php_zvals_not_of_type(IS_STRING TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    assert_datastore_instance_equals_destroy(
        "invalid scheme",
        &((nr_datastore_instance_t){
            .database_name = default_database,
            .host = system_host_name,
            .port_path_or_id = default_port,
        }),
        nr_predis_create_datastore_instance_from_fields(invalid_zvals[i], NULL,
                                                        NULL, NULL, NULL));
  }
  tlib_php_free_zval_array(&invalid_zvals);

  /*
   * Test : Invalid hosts.
   */
  invalid_zvals = tlib_php_zvals_not_of_type(IS_STRING TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    assert_datastore_instance_equals_destroy(
        "invalid host",
        &((nr_datastore_instance_t){
            .database_name = default_database,
            .host = system_host_name,
            .port_path_or_id = default_port,
        }),
        nr_predis_create_datastore_instance_from_fields(NULL, invalid_zvals[i],
                                                        NULL, NULL, NULL));
  }
  tlib_php_free_zval_array(&invalid_zvals);

  /*
   * Test : Invalid ports.
   */
  invalid_zvals = tlib_php_zvals_not_of_type(IS_LONG TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    assert_datastore_instance_equals_destroy(
        "invalid port",
        &((nr_datastore_instance_t){
            .database_name = default_database,
            .host = system_host_name,
            .port_path_or_id = default_port,
        }),
        nr_predis_create_datastore_instance_from_fields(
            NULL, NULL, invalid_zvals[i], NULL, NULL));
  }
  tlib_php_free_zval_array(&invalid_zvals);

  /*
   * Test : Invalid paths.
   */
  scheme = tlib_php_request_eval_expr("'unix'" TSRMLS_CC);
  invalid_zvals = tlib_php_zvals_not_of_type(IS_STRING TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    assert_datastore_instance_equals_destroy(
        "invalid path",
        &((nr_datastore_instance_t){
            .database_name = default_database,
            .host = system_host_name,
            .port_path_or_id = "unknown",
        }),
        nr_predis_create_datastore_instance_from_fields(
            scheme, NULL, NULL, invalid_zvals[i], NULL));
  }
  tlib_php_free_zval_array(&invalid_zvals);
  nr_php_zval_free(&scheme);

  /*
   * Test : Invalid databases.
   */
  assert_datastore_instance_equals_destroy(
      "database as array",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = "array()",
      }) TSRMLS_CC));

  assert_datastore_instance_equals_destroy(
      "database as object",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      create_datastore_instance_from_fields(&((field_literals_t){
          .scheme = NULL,
          .host = NULL,
          .port = NULL,
          .path = NULL,
          .database = "new \\stdClass",
      }) TSRMLS_CC));

  tlib_php_request_end();
}

/*
 * As the functions being unit tested below all defer to
 * nr_predis_create_datastore_instance_from_fields() to do the heavy lifting,
 * the next few functions (test_create_datastore_instance_*) only exist to test
 * the parameter handling in each wrapper function, and do not attempt to
 * exhaustively test all possible datastore instance permutations as
 * test_create_datastore_instance_from_fields() does above.
 */
static void test_create_datastore_instance_from_array(TSRMLS_D) {
  zval* input = NULL;

  tlib_php_request_start();

  input = tlib_php_request_eval_expr("array()" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "empty array",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_create_datastore_instance_from_array(input));
  nr_php_zval_free(&input);

  input = tlib_php_request_eval_expr(
      "array('scheme' => 'unix', 'path' => '/tmp/redis.sock')" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "unix array",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_predis_create_datastore_instance_from_array(input));
  nr_php_zval_free(&input);

  input = tlib_php_request_eval_expr(
      "array('scheme' => 'tcp', 'host' => 'foo.bar', 'port' => 9999, "
      "'database' => 1)" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "tcp array",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = "foo.bar",
          .port_path_or_id = "9999",
      }),
      nr_predis_create_datastore_instance_from_array(input));
  nr_php_zval_free(&input);

  tlib_php_request_end();
}

static void test_create_datastore_instance_from_parameters_object(
    const char* ns,
    const char* iface,
    const char* klass TSRMLS_DC) {
  zval* input = NULL;
  char* source;

  tlib_php_request_start();
  declare_parameters_class(ns, iface, klass TSRMLS_CC);

  source = nr_formatf("new \\%s\\%s(null, null, null, null, null)", ns, klass);
  input = tlib_php_request_eval_expr(source TSRMLS_CC);
  nr_free(source);
  assert_datastore_instance_equals_destroy(
      "empty object",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_create_datastore_instance_from_parameters_object(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  source = nr_formatf(
      "new \\%s\\%s('unix', null, null, '/tmp/redis.sock', null)", ns, klass);
  input = tlib_php_request_eval_expr(source TSRMLS_CC);
  nr_free(source);
  assert_datastore_instance_equals_destroy(
      "unix object",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_predis_create_datastore_instance_from_parameters_object(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  source
      = nr_formatf("new \\%s\\%s('tcp', 'foo.bar', 9999, null, 1)", ns, klass);
  input = tlib_php_request_eval_expr(source TSRMLS_CC);
  nr_free(source);
  assert_datastore_instance_equals_destroy(
      "tcp object",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = "foo.bar",
          .port_path_or_id = "9999",
      }),
      nr_predis_create_datastore_instance_from_parameters_object(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  tlib_php_request_end();
}

static void test_create_datastore_instance_from_string(TSRMLS_D) {
  zval* input = NULL;

  tlib_php_request_start();

  /*
   * Test : Invalid URL.
   */
  input = tlib_php_request_eval_expr("':'" TSRMLS_CC);
  tlib_pass_if_null(
      "invalid URL",
      nr_predis_create_datastore_instance_from_string(input TSRMLS_CC));
  nr_php_zval_free(&input);

  /*
   * Test : Normal operation.
   */
  input
      = tlib_php_request_eval_expr("'unix://foo.bar/tmp/redis.sock'" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "unix string",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_predis_create_datastore_instance_from_string(input TSRMLS_CC));
  nr_php_zval_free(&input);

  input
      = tlib_php_request_eval_expr("'tcp://foo.bar:9999?database=1'" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "tcp array",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = "foo.bar",
          .port_path_or_id = "9999",
      }),
      nr_predis_create_datastore_instance_from_string(input TSRMLS_CC));
  nr_php_zval_free(&input);

  tlib_php_request_end();
}

static void test_create_datastore_instance_from_connection_params(
    const char* ns,
    const char* iface,
    const char* klass TSRMLS_DC) {
  size_t i;
  zval** invalid_zvals;
  zval* input = NULL;
  char* source;

  tlib_php_request_start();
  declare_parameters_class(ns, iface, klass TSRMLS_CC);

  /*
   * Since the function being tested is basically just a big switch based on
   * type that then delegates to other functions, we'll feed in the different
   * possible inputs and make sure we get sensible looking output.
   */
  input = tlib_php_request_eval_expr(
      "'unix://foo.bar/tmp/redis.sock?database=1'" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "string",
      &((nr_datastore_instance_t){
          .database_name = "1",
          .host = system_host_name,
          .port_path_or_id = "/tmp/redis.sock",
      }),
      nr_predis_create_datastore_instance_from_connection_params(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  input = tlib_php_request_eval_expr("array('host' => 'array')" TSRMLS_CC);
  assert_datastore_instance_equals_destroy(
      "array",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "array",
          .port_path_or_id = default_port,
      }),
      nr_predis_create_datastore_instance_from_connection_params(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  source = nr_formatf("new \\%s\\%s(null, null, 9999, null, null)", ns, klass);
  input = tlib_php_request_eval_expr(source TSRMLS_CC);
  nr_free(source);
  assert_datastore_instance_equals_destroy(
      "parameters object",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = "9999",
      }),
      nr_predis_create_datastore_instance_from_connection_params(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  source = nr_formatf(
      "function () { return new \\%s\\%s(null, 'callable', null, null, null); "
      "}",
      ns, klass);
  input = tlib_php_request_eval_expr(source TSRMLS_CC);
  nr_free(source);
  assert_datastore_instance_equals_destroy(
      "parameters object",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = "callable",
          .port_path_or_id = default_port,
      }),
      nr_predis_create_datastore_instance_from_connection_params(
          input TSRMLS_CC));
  nr_php_zval_free(&input);

  /*
   * NULL is a totally valid input as well.
   */
  assert_datastore_instance_equals_destroy(
      "no parameters",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_create_datastore_instance_from_connection_params(
          NULL TSRMLS_CC));

  /*
   * Test : Failure cases.
   */
  invalid_zvals = tlib_php_zvals_not_of_type(IS_STRING TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    assert_datastore_instance_equals_destroy(
        "invalid connection params",
        &((nr_datastore_instance_t){
            .database_name = default_database,
            .host = system_host_name,
            .port_path_or_id = default_port,
        }),
        nr_predis_create_datastore_instance_from_connection_params(
            invalid_zvals[i] TSRMLS_CC));
  }
  tlib_php_free_zval_array(&invalid_zvals);

  tlib_php_request_end();
}

static void test_get_operation_name_from_object(TSRMLS_D) {
  char* res;
  zval* obj;

  tlib_php_request_start();

  tlib_php_request_eval(
      "namespace Predis\\Command;"
      "interface CommandInterface { public function getId(); }"
      "class Command implements CommandInterface {"
      "protected $id;"
      "public function __construct($id) { $this->id = $id; }"
      "public function getId() { return $this->id; }"
      "}" TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL command",
                    nr_predis_get_operation_name_from_object(NULL TSRMLS_CC));

  /*
   * We'll do a basic test here of the object, but leave it to the tests for
   * nr_predis_is_command() to test all the possible cases more thoroughly.
   */
  obj = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);
  tlib_pass_if_null("invalid command",
                    nr_predis_get_operation_name_from_object(obj TSRMLS_CC));
  nr_php_zval_free(&obj);

  /*
   * Test : Normal operation.
   */
  obj = tlib_php_request_eval_expr(
      "new \\Predis\\Command\\Command('FOO')" TSRMLS_CC);
  res = nr_predis_get_operation_name_from_object(obj TSRMLS_CC);
  tlib_pass_if_str_equal("valid command", "foo", res);
  nr_free(res);
  nr_php_zval_free(&obj);

  /*
   * Test : Predis <= 0.7.
   */
  tlib_php_request_eval(
      "namespace Predis\\Commands;"
      "interface ICommand { public function getId(); }"
      "class Command implements ICommand {"
      "protected $id;"
      "public function __construct($id) { $this->id = $id; }"
      "public function getId() { return $this->id; }"
      "}" TSRMLS_CC);

  obj = tlib_php_request_eval_expr(
      "new \\Predis\\Commands\\Command('FOO')" TSRMLS_CC);
  res = nr_predis_get_operation_name_from_object(obj TSRMLS_CC);
  tlib_pass_if_str_equal("valid command", "foo", res);
  nr_free(res);
  nr_php_zval_free(&obj);

  tlib_php_request_end();
}

static zval* instantiate_object(const char* ns, const char* klass TSRMLS_DC) {
  zval* obj;
  char* stmt = nr_formatf("new %s\\%s", ns, klass);

  obj = tlib_php_request_eval_expr(stmt TSRMLS_CC);

  nr_free(stmt);
  return obj;
}

static void test_is_method(int (*func)(const zval* obj TSRMLS_DC),
                           const char* ns,
                           const char* klass TSRMLS_DC) {
  zval* child;
  char* child_klass = nr_formatf("%sChild", klass);
  char* code;
  zval* other;
  zval* parent;

  code = nr_formatf(
      "namespace %s;"
      "class %s {}"
      "class %s extends %s {}",
      ns, klass, child_klass, klass);

  tlib_php_request_eval(code TSRMLS_CC);
  child = instantiate_object(ns, child_klass TSRMLS_CC);
  other = instantiate_object("", "stdClass" TSRMLS_CC);
  parent = instantiate_object(ns, klass TSRMLS_CC);

  tlib_pass_if_int_equal("NULL zval", 0, (func)(NULL TSRMLS_CC));
  tlib_pass_if_int_equal("stdClass", 0, (func)(other TSRMLS_CC));
  tlib_fail_if_int_equal("parent object", 0, (func)(parent TSRMLS_CC));
  tlib_fail_if_int_equal("child object", 0, (func)(child TSRMLS_CC));

  nr_php_zval_free(&child);
  nr_php_zval_free(&other);
  nr_php_zval_free(&parent);
  nr_free(child_klass);
  nr_free(code);
}

static void test_is_methods(TSRMLS_D) {
  tlib_php_request_start();

  test_is_method(nr_predis_is_aggregate_connection, "Predis\\Connection",
                 "AggregateConnectionInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_aggregate_connection, "Predis\\Connection",
                 "AggregatedConnectionInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_aggregate_connection, "Predis\\Network",
                 "IConnectionCluster" TSRMLS_CC);
  test_is_method(nr_predis_is_command, "Predis\\Command",
                 "CommandInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_command, "Predis\\Commands",
                 "ICommand" TSRMLS_CC);
  test_is_method(nr_predis_is_connection, "Predis\\Connection",
                 "ConnectionInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_connection, "Predis\\Network",
                 "IConnection" TSRMLS_CC);
  test_is_method(nr_predis_is_node_connection, "Predis\\Connection",
                 "NodeConnectionInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_node_connection, "Predis\\Connection",
                 "SingleConnectionInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_node_connection, "Predis\\Network",
                 "IConnectionSingle" TSRMLS_CC);
  test_is_method(nr_predis_is_parameters, "Predis\\Connection",
                 "ConnectionParametersInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_parameters, "Predis\\Connection",
                 "ParametersInterface" TSRMLS_CC);
  test_is_method(nr_predis_is_parameters, "Predis",
                 "IConnectionParameters" TSRMLS_CC);

  tlib_php_request_end();
}

static void test_retrieve_datastore_instance(TSRMLS_D) {
  nr_datastore_instance_t* instance;
  zval* predis;

  tlib_php_request_start();

  /*
   * We don't check the object type at this level, so we'll just use a stdClass
   * instance to stand in for the Predis connection.
   */
  predis = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL conn",
                    nr_predis_retrieve_datastore_instance(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null("unsaved conn",
                    nr_predis_retrieve_datastore_instance(predis TSRMLS_CC));

  instance = nr_predis_save_datastore_instance(predis, NULL TSRMLS_CC);
  tlib_pass_if_ptr_equal(
      "saved conn", instance,
      nr_predis_retrieve_datastore_instance(predis TSRMLS_CC));

  nr_php_zval_free(&predis);
  tlib_php_request_end();
}

static void test_save_datastore_instance(TSRMLS_D) {
  zval* a;
  zval* b;

  tlib_php_request_start();

  /*
   * We don't check the object type at this level, so we'll just use a stdClass
   * instance to stand in for the Predis connection.
   */
  a = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);
  b = tlib_php_request_eval_expr("new \\stdClass" TSRMLS_CC);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL connection",
                    nr_predis_save_datastore_instance(NULL, NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  assert_datastore_instance_equals(
      "first connection",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_save_datastore_instance(a, NULL TSRMLS_CC));
  tlib_pass_if_size_t_equal("first connection", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  assert_datastore_instance_equals(
      "second connection",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_save_datastore_instance(b, NULL TSRMLS_CC));
  tlib_pass_if_size_t_equal("second connection", 2,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  assert_datastore_instance_equals(
      "updated connection",
      &((nr_datastore_instance_t){
          .database_name = default_database,
          .host = system_host_name,
          .port_path_or_id = default_port,
      }),
      nr_predis_save_datastore_instance(b, NULL TSRMLS_CC));
  tlib_pass_if_size_t_equal("second connection", 2,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_zval_free(&a);
  nr_php_zval_free(&b);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  default_database = nr_formatf("%ld", (long)nr_predis_default_database);
  default_port = nr_formatf("%ld", (long)nr_predis_default_port);
  system_host_name = nr_system_get_hostname();

  tlib_php_engine_create("" PTSRMLS_CC);

  test_create_datastore_instance_from_fields(TSRMLS_C);
  test_create_datastore_instance_from_array(TSRMLS_C);
  test_create_datastore_instance_from_string(TSRMLS_C);

  // Version 0.8.
  test_create_datastore_instance_from_parameters_object(
      "Predis\\Connection", "ConnectionParametersInterface",
      "ConnectionParameters" TSRMLS_CC);
  // Version 1.x.
  test_create_datastore_instance_from_parameters_object(
      "Predis\\Connection", "ParametersInterface", "Parameters" TSRMLS_CC);

  // Version 0.8.
  test_create_datastore_instance_from_connection_params(
      "Predis\\Connection", "ConnectionParametersInterface",
      "ConnectionParameters" TSRMLS_CC);
  // Version 1.x.
  test_create_datastore_instance_from_connection_params(
      "Predis\\Connection", "ParametersInterface", "Parameters" TSRMLS_CC);

  test_get_operation_name_from_object(TSRMLS_C);
  test_is_methods(TSRMLS_C);
  test_retrieve_datastore_instance(TSRMLS_C);
  test_save_datastore_instance(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);

  nr_free(default_database);
  nr_free(default_port);
  nr_free(system_host_name);
}
