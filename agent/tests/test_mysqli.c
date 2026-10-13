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
#include "php_objtable.h"
#include "php_txn.h"
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
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_php_mysqli_save_datastore_instance(NULL, "", 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null conn and empty host", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  /*
   * Test: Normal operation
   */
  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysqli");

  nr_php_mysqli_save_datastore_instance(conn, NULL, 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "null host saves default instance", expected_default,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_php_mysqli_save_datastore_instance(conn, "blue", 0, NULL, NULL TSRMLS_CC);
  assert_datastore_instance_equals(
      "same conn saves new instance", expected,
      nr_hashmap_get(NRPRG_CTX(datastore_connections), key, nr_strlen(key)));

  nr_hashmap_delete(NRPRG_CTX(datastore_connections), key, nr_strlen(key));
  nr_php_mysqli_save_datastore_instance(conn, "blue", 0, NULL, NULL TSRMLS_CC);
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

  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
                 nr_php_mysqli_create_datastore_instance(NULL, 0, NULL, NULL));
  assert_datastore_instance_equals(
      "connection info is found", expected,
      nr_php_mysqli_retrieve_datastore_instance(NULL TSRMLS_CC));

  nr_free(key);
  key = nr_php_datastore_make_key(conn, "mysqli");

  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
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
  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
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

  nr_hashmap_set(NRPRG_CTX(datastore_connections), key, nr_strlen(key),
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

/*
 * A stdClass stands in for a mysqli link: the functions under test use only
 * the object handle.
 */
static void set_explain_options(nrtxn_t* txn,
                                int ep_enabled,
                                nr_tt_recordsql_t recordsql,
                                int slowsql) {
  txn->options.ep_enabled = ep_enabled;
  txn->options.tt_recordsql = recordsql;
  txn->options.tt_slowsql = slowsql;
}

static void test_link_metadata_gating(TSRMLS_D) {
  zval* link;
  nr_mysqli_metadata_link_t metadata;
  nrtxn_t* txn;

  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_php_request_eval("$link = new stdClass;" TSRMLS_CC);
  link = tlib_php_request_eval_expr("$link" TSRMLS_CC);

  /* Nothing is kept when no explain plan can be produced. */
  set_explain_options(txn, 0, NR_SQL_OBFUSCATED, 1);
  tlib_pass_if_int_equal("explain off", 0, nr_php_mysqli_metadata_wanted());
  tlib_pass_if_status_failure("explain off: not saved",
                              nr_php_mysqli_metadata_set_connect(
                                  link, "h", "u", "pw", "d", 3306, NULL, 0));
  tlib_pass_if_status_failure("explain off: nothing to get",
                              nr_php_mysqli_metadata_get(link, &metadata));

  set_explain_options(txn, 1, NR_SQL_NONE, 1);
  tlib_pass_if_int_equal("record_sql off", 0, nr_php_mysqli_metadata_wanted());
  tlib_pass_if_status_failure("record_sql off: not saved",
                              nr_php_mysqli_metadata_set_connect(
                                  link, "h", "u", "pw", "d", 3306, NULL, 0));

  set_explain_options(txn, 1, NR_SQL_OBFUSCATED, 0);
  tlib_pass_if_int_equal("slow sql off", 0, nr_php_mysqli_metadata_wanted());
  tlib_pass_if_status_failure("slow sql off: not saved",
                              nr_php_mysqli_metadata_set_connect(
                                  link, "h", "u", "pw", "d", 3306, NULL, 0));
  tlib_pass_if_status_failure("other setters: not saved",
                              nr_php_mysqli_metadata_set_database(link, "x"));
  tlib_pass_if_status_failure("nothing to get",
                              nr_php_mysqli_metadata_get(link, &metadata));

  set_explain_options(txn, 1, NR_SQL_RAW, 1);
  tlib_pass_if_int_equal("raw", 1, nr_php_mysqli_metadata_wanted());
  set_explain_options(txn, 1, NR_SQL_OBFUSCATED, 1);
  tlib_pass_if_int_equal("obfuscated", 1, nr_php_mysqli_metadata_wanted());
  tlib_pass_if_status_success(
      "saved", nr_php_mysqli_metadata_set_connect(link, "h", "u", "pw", "d",
                                                  3306, NULL, 0));
  tlib_pass_if_status_success("get",
                              nr_php_mysqli_metadata_get(link, &metadata));
  tlib_pass_if_str_equal("host", "h", metadata.host);
  tlib_pass_if_str_equal("user", "u", metadata.user);
  tlib_pass_if_str_equal("password", "pw", metadata.password);

  nr_php_mysqli_metadata_remove(link);
  tlib_pass_if_status_failure("removed",
                              nr_php_mysqli_metadata_get(link, &metadata));

  nr_php_zval_free(&link);
  tlib_php_request_end();
}

/*
 * An update dropped because the transaction does not allow explain must not
 * leave the older entry behind: a later transaction would build the explain
 * connection from the old user and database.
 */
static void test_link_metadata_unwanted_removes_entry(TSRMLS_D) {
  zval* link;
  nr_mysqli_metadata_link_t metadata;
  nrtxn_t* txn;

  tlib_php_request_start();
  txn = NRPRG(txn);
  tlib_php_request_eval("$link = new stdClass;" TSRMLS_CC);
  link = tlib_php_request_eval_expr("$link" TSRMLS_CC);

  set_explain_options(txn, 1, NR_SQL_OBFUSCATED, 1);
  nr_php_mysqli_metadata_set_connect(link, "h", "a", "pwa", "x", 3306, NULL, 0);
  tlib_pass_if_status_success("saved",
                              nr_php_mysqli_metadata_get(link, &metadata));

  set_explain_options(txn, 0, NR_SQL_OBFUSCATED, 1);
  tlib_pass_if_status_failure(
      "change_user in a transaction that does not allow explain",
      nr_php_mysqli_metadata_set_user(link, "b", "pwb", "y"));
  set_explain_options(txn, 1, NR_SQL_OBFUSCATED, 1);
  tlib_pass_if_status_failure("the old entry is gone",
                              nr_php_mysqli_metadata_get(link, &metadata));

  nr_php_zval_free(&link);
  tlib_php_request_end();
}

/*
 * A setter can make an entry for a link that was opened while the agent was
 * not recording. Such an entry has no host or user and is not used. Options
 * and TLS settings set before the connect, as mysqli_init() allows, are kept.
 */
static void test_link_metadata_complete(TSRMLS_D) {
  zval* link;
  zval* other;
  nr_mysqli_metadata_link_t metadata;

  tlib_php_request_start();
  set_explain_options(NRPRG(txn), 1, NR_SQL_OBFUSCATED, 1);
  tlib_php_request_eval(
      "$link = new stdClass; $other = new stdClass;" TSRMLS_CC);
  link = tlib_php_request_eval_expr("$link" TSRMLS_CC);
  other = tlib_php_request_eval_expr("$other" TSRMLS_CC);

  nr_php_mysqli_metadata_set_database(link, "db");
  tlib_pass_if_status_failure("select_db only: not usable",
                              nr_php_mysqli_metadata_get(link, &metadata));
  nr_php_mysqli_metadata_set_user(link, "u", "pw", "db");
  tlib_pass_if_status_failure("change_user only: not usable",
                              nr_php_mysqli_metadata_get(link, &metadata));

  nr_php_mysqli_metadata_set_option(other, 7, "opt");
  nr_php_mysqli_metadata_set_ssl(other, "key", NULL, NULL, NULL, NULL);
  tlib_pass_if_status_failure("options and TLS only: not usable",
                              nr_php_mysqli_metadata_get(other, &metadata));
  nr_php_mysqli_metadata_set_connect(other, "h", "u", "pw", "db", 3306, NULL,
                                     0);
  tlib_pass_if_status_success("connected: usable",
                              nr_php_mysqli_metadata_get(other, &metadata));
  tlib_pass_if_not_null("options kept", metadata.options);
  tlib_pass_if_str_equal("ssl key kept", "key", metadata.ssl_key);

  nr_php_zval_free(&link);
  nr_php_zval_free(&other);
  tlib_php_request_end();
}

/*
 * A second connect on the same link replaces the connection parameters, so
 * an omitted password or database does not keep the earlier one.
 */
static void test_link_metadata_reconnect(TSRMLS_D) {
  zval* link;
  nr_mysqli_metadata_link_t metadata;

  tlib_php_request_start();
  set_explain_options(NRPRG(txn), 1, NR_SQL_OBFUSCATED, 1);
  tlib_php_request_eval("$link = new stdClass;" TSRMLS_CC);
  link = tlib_php_request_eval_expr("$link" TSRMLS_CC);

  nr_php_mysqli_metadata_set_connect(link, "h", "a", "pwa", "db1", 3306, NULL,
                                     0);
  nr_php_mysqli_metadata_set_connect(link, "h", "b", NULL, NULL, 3306, NULL, 0);
  tlib_pass_if_status_success("get",
                              nr_php_mysqli_metadata_get(link, &metadata));
  tlib_pass_if_str_equal("user", "b", metadata.user);
  tlib_pass_if_null("password", metadata.password);
  tlib_pass_if_null("database", metadata.database);

  nr_php_zval_free(&link);
  tlib_php_request_end();
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO

/*
 * On PHP 8.0 and later the metadata of a link lasts for the request, not the
 * transaction, and goes away when the link object is freed. A new object that
 * takes the freed object's handle finds nothing.
 */
static void test_link_metadata_lifetime(TSRMLS_D) {
  zval* first;
  zval* second;
  nr_mysqli_metadata_link_t metadata;
  nrtxn_t* txn;

  tlib_php_request_start();
  txn = NRPRG(txn);
  set_explain_options(txn, 1, NR_SQL_OBFUSCATED, 1);

  tlib_pass_if_not_null("the store exists", nr_php_mysqli_links());
  tlib_pass_if_not_null("the table exists", NRPRG_SHARED(mysqli_link_handles));

  tlib_php_request_eval(
      "$first = new stdClass; $second = new stdClass;" TSRMLS_CC);
  first = tlib_php_request_eval_expr("$first" TSRMLS_CC);
  second = tlib_php_request_eval_expr("$second" TSRMLS_CC);

  nr_php_mysqli_metadata_set_connect(first, "one", "u1", "pw1", NULL, 3306,
                                     NULL, 0);
  nr_php_mysqli_metadata_set_connect(second, "two", "u2", "pw2", NULL, 3306,
                                     NULL, 0);
  nr_php_mysqli_metadata_set_database(first, "db1");
  tlib_pass_if_size_t_equal(
      "one table entry per link, however often it is set", 2,
      nr_php_objtable_count(NRPRG_SHARED(mysqli_link_handles)));

  /* A save that fails its arguments does not track the link. */
  tlib_pass_if_status_failure(
      "no database", nr_php_mysqli_metadata_set_database(second, NULL));
  tlib_pass_if_size_t_equal(
      "no table entry for a failed save", 2,
      nr_php_objtable_count(NRPRG_SHARED(mysqli_link_handles)));

  /* The store outlives the transaction. */
  nr_php_txn_end(0, 0);
  tlib_pass_if_status_success("kept after the transaction ended",
                              nr_php_mysqli_metadata_get(first, &metadata));
  tlib_pass_if_str_equal("password kept", "pw1", metadata.password);
  tlib_pass_if_str_equal("database kept", "db1", metadata.database);

  /* Closing a link removes its entry only. */
  nr_php_mysqli_metadata_remove(second);
  tlib_pass_if_status_failure("closed link",
                              nr_php_mysqli_metadata_get(second, &metadata));
  tlib_pass_if_status_success("other link kept",
                              nr_php_mysqli_metadata_get(first, &metadata));
  tlib_pass_if_size_t_equal(
      "table entry removed with the link", 1,
      nr_php_objtable_count(NRPRG_SHARED(mysqli_link_handles)));

  /* Freeing a link removes its entry and a reused handle finds nothing. */
  tlib_php_request_eval("unset($first);" TSRMLS_CC);
  nr_php_zval_free(&first);
  tlib_php_request_eval(
      "$reuse = []; for ($i = 0; $i < 50; $i++) { $o = new stdClass; $reuse[] "
      "= $o; }" TSRMLS_CC);
  first = tlib_php_request_eval_expr("$reuse[0]" TSRMLS_CC);
  tlib_pass_if_size_t_equal(
      "the table is empty after the link was freed", 0,
      nr_php_objtable_count(NRPRG_SHARED(mysqli_link_handles)));
  {
    zval* object;
    int i;

    for (i = 0; i < 50; i++) {
      char expr[64];

      snprintf(expr, sizeof(expr), "$reuse[%d]", i);
      object = tlib_php_request_eval_expr(expr TSRMLS_CC);
      tlib_pass_if_status_failure(
          "no object finds the freed link's metadata",
          nr_php_mysqli_metadata_get(object, &metadata));
      nr_php_zval_free(&object);
    }
  }

  nr_php_zval_free(&first);
  nr_php_zval_free(&second);
  tlib_php_request_end();
}

#endif /* PHP 8.0+ */

static void test_option_to_string(TSRMLS_D) {
  struct {
    const char* expr;
    const char* expected; /* NULL: not captured */
  } cases[] = {
      {"'abc'", "abc"},       {"''", ""},     {"42", "42"},
      {"true", "1"},          {"false", "0"}, {"1.5", "1.5"},
      {"1.0", "1"},           {"null", ""},   {"[1]", NULL},
      {"new stdClass", NULL},
  };
  size_t i;

  tlib_pass_if_null("NULL value", nr_php_mysqli_option_to_string(NULL));

  tlib_php_request_start();
  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    zval* value = tlib_php_request_eval_expr(cases[i].expr TSRMLS_CC);
    char* term = nr_php_mysqli_option_to_string(value);

    if (cases[i].expected) {
      tlib_pass_if_str_equal(cases[i].expr, cases[i].expected, term);
    } else {
      tlib_pass_if_null(cases[i].expr, term);
    }
    nr_free(term);
    nr_php_zval_free(&value);
  }
  tlib_php_request_end();
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
  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("mysqli.default_socket=" DEFAULT_SOCKET PTSRMLS_CC);

  if (tlib_php_require_extension("mysqli" TSRMLS_CC)) {
    test_save_datastore_instance(TSRMLS_C);
    test_retrieve_datastore_instance(TSRMLS_C);
    test_remove_datastore_instance(TSRMLS_C);
    test_default_port_host_and_socket();
    test_get_host_and_port_path_or_id();
    test_create_datastore_instance();
    test_link_metadata_gating(TSRMLS_C);
    test_link_metadata_unwanted_removes_entry(TSRMLS_C);
    test_link_metadata_complete(TSRMLS_C);
    test_link_metadata_reconnect(TSRMLS_C);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
    test_link_metadata_lifetime(TSRMLS_C);
#endif
    test_option_to_string(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);

  test_strip_persistent_prefix();
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
