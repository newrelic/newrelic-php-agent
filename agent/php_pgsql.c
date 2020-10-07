/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_postgres.h"
#include "php_agent.h"
#include "php_datastore.h"
#include "php_pgsql.h"
#include "php_pgsql_private.h"
#include "util_logging.h"

nr_datastore_instance_t* nr_php_pgsql_create_datastore_instance(
    const char* conn_info) {
  nr_datastore_instance_t* instance = NULL;
  char* host = NULL;
  char* port_path_or_id = NULL;
  char* database_name = NULL;

  nr_postgres_parse_conn_info(conn_info, &host, &port_path_or_id,
                              &database_name);
  instance = nr_datastore_instance_create(host, port_path_or_id, database_name);

  nr_free(host);
  nr_free(port_path_or_id);
  nr_free(database_name);

  return instance;
}

void nr_php_pgsql_save_datastore_instance(const zval* pgsql_conn,
                                          const char* conn_info TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = nr_php_datastore_make_key(pgsql_conn, "pgsql");

  if (nr_php_datastore_has_conn(key TSRMLS_CC)) {
    nr_free(key);
    return;
  }

  instance = nr_php_pgsql_create_datastore_instance(conn_info);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(NRPRG(pgsql_last_conn));
  NRPRG(pgsql_last_conn) = key;
}

nr_datastore_instance_t* nr_php_pgsql_retrieve_datastore_instance(
    const zval* pgsql_conn TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = NULL;

  if (NULL == pgsql_conn) {
    /*
     * If we have an existing connection, use that as the key. Otherwise,
     * create a default pgsql instance and make a key from the NULL zval.
     */
    if (NRPRG(pgsql_last_conn)) {
      key = nr_strdup(NRPRG(pgsql_last_conn));
    } else {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "could not find previous pgsql connection");
      nr_php_pgsql_save_datastore_instance(pgsql_conn, NULL TSRMLS_CC);
      key = nr_php_datastore_make_key(pgsql_conn, "pgsql");
    }
  } else {
    key = nr_php_datastore_make_key(pgsql_conn, "pgsql");
  }

  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);
  nr_free(key);

  return instance;
}

void nr_php_pgsql_remove_datastore_instance(const zval* pgsql_conn TSRMLS_DC) {
  char* key = NULL;

  /*
   * If the connection is NULL but we have an existing connection, use that as
   * the key. Otherwise, make a key from the connection zval.
   */
  if ((NULL == pgsql_conn) && (NRPRG(pgsql_last_conn))) {
    key = nr_strdup(NRPRG(pgsql_last_conn));
  } else {
    key = nr_php_datastore_make_key(pgsql_conn, "pgsql");
  }

  nr_php_datastore_instance_remove(key TSRMLS_CC);

  if (0 == nr_strcmp(key, NRPRG(pgsql_last_conn))) {
    nr_free(NRPRG(pgsql_last_conn));
  }

  nr_free(key);
}
