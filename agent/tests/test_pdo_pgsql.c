/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_datastore.h"
#include "tlib_php.h"

#include "nr_datastore_instance.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_pdo_pgsql.h"
#include "util_system.h"

static char* system_host_name;

#define DEFAULT_DATABASE_NAME "uhura"
#define DEFAULT_PORT "5432"
#define DEFAULT_SOCKET "/tmp"

static void assert_dsn_instance_f(const char* message,
                                  const nr_datastore_instance_t* expected,
                                  const char* dsn,
                                  const char* file,
                                  int line TSRMLS_DC) {
  nr_datastore_instance_t* actual;
  char* dsn_copy = nr_strdup(dsn);
  pdo_driver_t driver = {
      .driver_name = "pgsql",
      .driver_name_len = 5,
  };
  pdo_dbh_t dbh = {
      .driver = &driver,
      .data_source = dsn_copy,
      .data_source_len = nr_strlen(dsn_copy),
      .username = DEFAULT_DATABASE_NAME,
  };

  actual = nr_php_pdo_pgsql_create_datastore_instance(&dbh TSRMLS_CC);

  assert_datastore_instance_equals_f(message, expected, actual, file, line);

  nr_datastore_instance_destroy(&actual);
  nr_free(dsn_copy);
}

#define assert_dsn_instance(MSG, EXPECTED, DSN) \
  assert_dsn_instance_f((MSG), (EXPECTED), (DSN), __FILE__, __LINE__ TSRMLS_CC)

static void test_create_datastore_instance(TSRMLS_D) {
  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dbh",
                    nr_php_pdo_pgsql_create_datastore_instance(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  assert_dsn_instance("empty DSN",
                      &((nr_datastore_instance_t){
                          .database_name = DEFAULT_DATABASE_NAME,
                          .host = system_host_name,
                          .port_path_or_id = DEFAULT_SOCKET,
                      }),
                      "");

  assert_dsn_instance("host only",
                      &((nr_datastore_instance_t){
                          .database_name = DEFAULT_DATABASE_NAME,
                          .host = system_host_name,
                          .port_path_or_id = DEFAULT_PORT,
                      }),
                      "host=127.0.0.1");

  assert_dsn_instance("port only",
                      &((nr_datastore_instance_t){
                          .database_name = DEFAULT_DATABASE_NAME,
                          .host = system_host_name,
                          .port_path_or_id = "4444",
                      }),
                      "port=4444");

  assert_dsn_instance("dbname only",
                      &((nr_datastore_instance_t){
                          .database_name = "db",
                          .host = system_host_name,
                          .port_path_or_id = DEFAULT_SOCKET,
                      }),
                      "dbname=db");

  assert_dsn_instance("host and port",
                      &((nr_datastore_instance_t){
                          .database_name = DEFAULT_DATABASE_NAME,
                          .host = system_host_name,
                          .port_path_or_id = "5431",
                      }),
                      "host=127.0.0.1 port=5431");

  assert_dsn_instance("all fields set",
                      &((nr_datastore_instance_t){
                          .database_name = "db",
                          .host = system_host_name,
                          .port_path_or_id = "5431",
                      }),
                      "host=localhost port=5431 dbname=db");
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  system_host_name = nr_system_get_hostname();
  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("pdo_pgsql" TSRMLS_CC)) {
    test_create_datastore_instance(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
  nr_free(system_host_name);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
