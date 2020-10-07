/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdlib.h>

#include "nr_postgres.h"
#include "nr_postgres_private.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

#define DEFAULT_DATABASE_NAME "unknown"
#define DEFAULT_PORT "5432"
#define DEFAULT_SOCKET "/tmp"

static void test_default_port_host_and_socket(void) {
  char* host = NULL;
  char* port = NULL;
  char* database_name = NULL;

  unsetenv("PGHOST");
  unsetenv("PGHOSTADDR");
  unsetenv("PGPORT");
  unsetenv("PGUSER");
  unsetenv("PGDATABASE");

  /*
   * Test: Missing environment variables
   */
  host = nr_postgres_default_host();
  tlib_pass_if_str_equal("default host", "localhost", host);
  nr_free(host);

  port = nr_postgres_default_port();
  tlib_pass_if_str_equal("default port", DEFAULT_PORT, port);
  nr_free(port);

  database_name = nr_postgres_default_database_name();
  tlib_pass_if_str_equal("default database_name", "", database_name);
  nr_free(database_name);

  /*
   * Test: Environment variables: host
   */
  putenv("PGHOST=spock");
  host = nr_postgres_default_host();
  tlib_pass_if_str_equal("host", "spock", host);
  nr_free(host);

  putenv("PGHOSTADDR=kirk");
  host = nr_postgres_default_host();
  tlib_pass_if_str_equal("hostaddr has precedence over host", "kirk", host);
  nr_free(host);

  unsetenv("PGHOST");
  unsetenv("PGHOSTADDR");

  /*
   * Test: Environment variables: port
   */
  putenv("PGPORT=2468");
  port = nr_postgres_default_port();
  tlib_pass_if_str_equal("port", "2468", port);
  nr_free(port);

  unsetenv("PGPORT");

  /*
   * Test: Environment variables: database_name
   */
  putenv("PGUSER=uhura");
  database_name = nr_postgres_default_database_name();
  tlib_pass_if_str_equal("user is default database name", "uhura",
                         database_name);
  nr_free(database_name);

  putenv("PGDATABASE=scotty");
  database_name = nr_postgres_default_database_name();
  tlib_pass_if_str_equal("dbname has precedence over user", "scotty",
                         database_name);
  nr_free(database_name);

  unsetenv("PGUSER");
  unsetenv("PGDATABASE");
}

static void test_conn_info_early_return(void) {
  char* host = nr_strdup("no");
  char* port_path_or_id = nr_strdup("nope");
  char* database_name = nr_strdup("negatory");

  /*
   * Test: Non-NULL return value params don't blow up
   */
  nr_postgres_parse_conn_info("", &host, &port_path_or_id, &database_name);

  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(database_name);
}

static void test_conn_info(const char* conn_info,
                           const char* expected_host,
                           const char* expected_port_path_or_id,
                           const char* expected_database_name) {
  char* host = NULL;
  char* port_path_or_id = NULL;
  char* database_name = NULL;

  nr_postgres_parse_conn_info(conn_info, &host, &port_path_or_id,
                              &database_name);
  tlib_pass_if_str_equal("correct host", expected_host, host);
  tlib_pass_if_str_equal("correct port_path_or_id", expected_port_path_or_id,
                         port_path_or_id);
  tlib_pass_if_str_equal("correct database_name", expected_database_name,
                         database_name);

  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(database_name);
}

static void test_parse_conn_info(void) {
  /*
   * Test: Bad conn_info
   */
  test_conn_info_early_return();
  test_conn_info(NULL, "localhost", DEFAULT_SOCKET, "");

  /*
   * Test: Nonsensical information
   */
  test_conn_info("host=/tmp port=4444", "localhost", DEFAULT_SOCKET, "");

  /*
   * Test: Missing information
   */
  test_conn_info("", "localhost", DEFAULT_SOCKET, "");
  test_conn_info(";", "localhost", DEFAULT_SOCKET, "");
  test_conn_info(";;", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("host=", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("host", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("hostaddr=", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("port=", "localhost", DEFAULT_PORT, "");
  test_conn_info("=5432", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("dbname=", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("user=", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("charset=UTF-8", "localhost", DEFAULT_SOCKET, "");

  /*
   * Test: Spaces, the final frontier
   */
  test_conn_info("host = localhost port = 5432 user = scotty", "localhost",
                 DEFAULT_PORT, "scotty");
  test_conn_info("host    =    localhost", "localhost", DEFAULT_PORT, "");
  test_conn_info("host=    localhost", "localhost", DEFAULT_PORT, "");
  test_conn_info("   port", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("port =       ", "localhost", DEFAULT_PORT, "");

  /*
   * Test: Localhost
   */
  test_conn_info("host=localhost", "localhost", DEFAULT_PORT, "");
  test_conn_info("hostaddr=localhost", "localhost", DEFAULT_PORT, "");
  test_conn_info("host=localhost port=1234", "localhost", "1234", "");
  test_conn_info("host=localhost port=/var/run/", "localhost", "/var/run/", "");
  test_conn_info("host=/tmp", "localhost", DEFAULT_SOCKET, "");
  test_conn_info("host=/tmp port=ignored", "localhost", DEFAULT_SOCKET, "");

  /*
   * Test: Precedence
   */
  test_conn_info("host=localhost hostaddr=127.0.0.1", "127.0.0.1", DEFAULT_PORT,
                 "");
  test_conn_info("user=uhura dbname=mccoy", "localhost", DEFAULT_SOCKET,
                 "mccoy");
  test_conn_info("host=localhost user=chekov", "localhost", DEFAULT_PORT,
                 "chekov");

  /*
   * Test: Non-localhost
   */
  test_conn_info("hostaddr=12.34.56.78", "12.34.56.78", DEFAULT_PORT, "");
  test_conn_info("host=spock port=5432 user=kirk password=enterprise", "spock",
                 DEFAULT_PORT, "kirk");
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_default_port_host_and_socket();
  test_parse_conn_info();
}
