/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_datastore_instance.h"
#include "nr_postgres.h"
#include "php_agent.h"
#include "php_pdo.h"
#include "php_pdo_pgsql.h"
#include "util_system.h"

/*
 * PDO converts the DSN into the connection info string expected by libpq:
 * https://github.com/php/php-src/blob/php-7.1.0/ext/pdo_pgsql/pgsql_driver.c#L1202-L1230
 *
 * Happily, we can just grab this string from the handler and pass it through
 * the axiom Postgres parser.
 */
nr_datastore_instance_t* nr_php_pdo_pgsql_create_datastore_instance(
    pdo_dbh_t* dbh TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* host = NULL;
  char* port_path_or_id = NULL;
  char* database_name = NULL;

  NR_UNUSED_TSRMLS;

  if (NULL == dbh) {
    return NULL;
  }

  nr_postgres_parse_conn_info(dbh->data_source, &host, &port_path_or_id,
                              &database_name);

  /*
   * Last ditch effort to populate database name if it wasn't provided in the
   * DSN, since the user name is used as a default.
   */
  if (nr_strempty(database_name)) {
    nr_free(database_name);
    database_name = nr_strdup(dbh->username);
  }

  instance = nr_datastore_instance_create(host, port_path_or_id, database_name);

  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(database_name);

  return instance;
}
