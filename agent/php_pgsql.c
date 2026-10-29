/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_postgres.h"
#include "php_agent.h"
#include "php_datastore.h"
#include "php_objtable.h"
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

void nr_php_pgsql_instance_destroy(void* instance) {
  nr_datastore_instance_t* i = instance;

  nr_datastore_instance_destroy(&i);
}

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
/*
 * The statements prepared on one connection, by statement name. The clock
 * orders the uses of the statements: each save and each lookup of a statement
 * gives it the next value, so the statement with the lowest value is the one
 * used least recently.
 */
typedef struct {
  char* sql;
  uint64_t used;

  /*
   * For the unnamed statement only: the start time of the transaction that
   * prepared it. The server destroys the unnamed statement with the next
   * query of almost any kind, so one that outlives its transaction is nearly
   * never the one the next pg_execute() runs. 0 for a named statement, and
   * for an unnamed one prepared while no transaction was active.
   */
  nrtime_t txn_start;
} nr_php_pgsql_statement_t;

typedef struct {
  nr_hashmap_t* names;
  uint64_t clock;
} nr_php_pgsql_statements_t;

static void nr_php_pgsql_statement_destroy(void* statement) {
  nr_php_pgsql_statement_t* s = statement;

  nr_free(s->sql);
  nr_free(s);
}

static nr_php_pgsql_statements_t* nr_php_pgsql_statements_create(void) {
  nr_php_pgsql_statements_t* statements
      = nr_malloc(sizeof(nr_php_pgsql_statements_t));

  statements->names = nr_hashmap_create(nr_php_pgsql_statement_destroy);
  statements->clock = 0;

  return statements;
}

void nr_php_pgsql_statements_destroy(void* statements) {
  nr_php_pgsql_statements_t* s = statements;

  nr_hashmap_destroy(&s->names);
  nr_free(s);
}
#endif

void nr_php_pgsql_save_datastore_instance(const zval* pgsql_conn,
                                          const char* conn_info TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = NULL;

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  /*
   * From PHP 8.1 a connection is an object, and PHP reuses an object's handle
   * once it is freed. The instance is kept in a table that drops it when the
   * object is freed, and every connect replaces what is there, so a reused
   * handle finds no entry.
   */
  if (nr_php_is_zval_valid_object(pgsql_conn)) {
    instance = nr_php_pgsql_create_datastore_instance(conn_info);

    if (NRPRG_SHARED(pgsql_instances)) {
      nr_php_objtable_set(NRPRG_SHARED(pgsql_instances), pgsql_conn, instance);
      nr_php_objtable_mark_last(NRPRG_SHARED(pgsql_instances), pgsql_conn);
    } else {
      nr_php_pgsql_instance_destroy(instance);
    }

    /*
     * A connect with the connection string of an open connection returns that
     * connection object, and the statements prepared on it are still there on
     * the server, so an entry that exists is kept. The table drops an entry
     * when its object is freed, so a connection that takes a freed
     * connection's handle never finds the statements of the earlier one.
     */
    if (NRPRG_SHARED(pgsql_statements)) {
      if (NULL
          == nr_php_objtable_get(NRPRG_SHARED(pgsql_statements), pgsql_conn)) {
        nr_php_objtable_set(NRPRG_SHARED(pgsql_statements), pgsql_conn,
                            nr_php_pgsql_statements_create());
      }
      nr_php_objtable_mark_last(NRPRG_SHARED(pgsql_statements), pgsql_conn);
    }
    return;
  }

  /*
   * Anything else that is not a resource is a failed connect (false). Only a
   * call that names no connection has a default instance to save, and a
   * failed connect must not create one.
   */
  if (NULL != pgsql_conn && !nr_php_is_zval_valid_resource(pgsql_conn)) {
    return;
  }
#endif

  key = nr_php_datastore_make_key(pgsql_conn, "pgsql");

  if (nr_php_datastore_has_conn(key TSRMLS_CC)) {
    nr_free(key);
    return;
  }

  instance = nr_php_pgsql_create_datastore_instance(conn_info);
  nr_php_datastore_instance_save(key, instance TSRMLS_CC);

  nr_free(NRPRG_CTX(pgsql_last_conn));
  NRPRG_CTX(pgsql_last_conn) = key;
}

nr_datastore_instance_t* nr_php_pgsql_retrieve_datastore_instance(
    const zval* pgsql_conn TSRMLS_DC) {
  nr_datastore_instance_t* instance = NULL;
  char* key = NULL;

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  if (nr_php_is_zval_valid_object(pgsql_conn)) {
    return nr_php_objtable_get(NRPRG_SHARED(pgsql_instances), pgsql_conn);
  }

  /* With no connection given, PHP uses the one connected last. */
  if (NULL == pgsql_conn) {
    instance = nr_php_objtable_get_last(NRPRG_SHARED(pgsql_instances));
    if (instance) {
      return instance;
    }
  }
#endif

  if (NULL == pgsql_conn) {
    /*
     * If we have an existing connection, use that as the key. Otherwise,
     * create a default pgsql instance and make a key from the NULL zval.
     */
    if (NRPRG_CTX(pgsql_last_conn)) {
      key = nr_strdup(NRPRG_CTX(pgsql_last_conn));
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

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  if (nr_php_is_zval_valid_object(pgsql_conn)) {
    nr_php_objtable_del(NRPRG_SHARED(pgsql_instances), pgsql_conn);
    nr_php_objtable_del(NRPRG_SHARED(pgsql_statements), pgsql_conn);
    return;
  }

  /*
   * With no connection given, PHP releases its default link and leaves the
   * connection open, so only the last mark goes. The entry goes with the
   * object.
   */
  if ((NULL == pgsql_conn)
      && nr_php_objtable_get_last(NRPRG_SHARED(pgsql_instances))) {
    nr_php_objtable_clear_last(NRPRG_SHARED(pgsql_instances));
    nr_php_objtable_clear_last(NRPRG_SHARED(pgsql_statements));
    return;
  }
#endif

  /*
   * If the connection is NULL but we have an existing connection, use that as
   * the key. Otherwise, make a key from the connection zval.
   */
  if ((NULL == pgsql_conn) && (NRPRG_CTX(pgsql_last_conn))) {
    key = nr_strdup(NRPRG_CTX(pgsql_last_conn));
  } else {
    key = nr_php_datastore_make_key(pgsql_conn, "pgsql");
  }

  nr_php_datastore_instance_remove(key TSRMLS_CC);

  if (0 == nr_strcmp(key, NRPRG_CTX(pgsql_last_conn))) {
    nr_free(NRPRG_CTX(pgsql_last_conn));
  }

  nr_free(key);
}

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
/*
 * The size of the buffer that holds the key of a statement. The statement names
 * are at most 63 bytes on the server, so the buffer is nearly always enough and
 * no allocation is made.
 */
#define NR_PHP_PGSQL_STATEMENT_KEY_BUFFER 128

/*
 * Purpose : Build the key of a statement in a buffer. The statements of a
 *           connection are in a map by statement name, and the key has a
 *           prefix so that the empty name, which PostgreSQL uses for the
 *           unnamed statement, is a valid key. A name that does not fit the
 *           buffer gets a heap key, which the caller frees.
 */

static const char* nr_php_pgsql_statement_key(const char* name,
                                              nr_string_len_t name_len,
                                              char* buffer,
                                              size_t buffer_len,
                                              char** heap_key,
                                              size_t* key_len) {
  size_t name_size = ((NULL != name) && (name_len > 0)) ? (size_t)name_len : 0;
  char* key = buffer;

  *heap_key = NULL;
  if (name_size + 3 > buffer_len) {
    key = (char*)nr_malloc(name_size + 3);
    *heap_key = key;
  }

  key[0] = 'n';
  key[1] = ':';
  if (name_size) {
    nr_memcpy(key + 2, name, name_size);
  }
  key[2 + name_size] = '\0';
  *key_len = 2 + name_size;

  return key;
}

static nr_php_pgsql_statements_t* nr_php_pgsql_statements_find(
    const zval* pgsql_conn,
    int create) {
  nr_php_pgsql_statements_t* statements;

  if (NULL == NRPRG_SHARED(pgsql_statements)) {
    return NULL;
  }

  /* With no connection given, PHP uses the one connected last. */
  if (NULL == pgsql_conn) {
    return nr_php_objtable_get_last(NRPRG_SHARED(pgsql_statements));
  }

  if (!nr_php_is_zval_valid_object(pgsql_conn)) {
    return NULL;
  }

  statements = nr_php_objtable_get(NRPRG_SHARED(pgsql_statements), pgsql_conn);
  if ((NULL == statements) && create) {
    /* A connection the agent did not see connect. */
    statements = nr_php_pgsql_statements_create();
    nr_php_objtable_set(NRPRG_SHARED(pgsql_statements), pgsql_conn, statements);

    /*
     * The table owns the statements, and drops them at once if it cannot
     * hold them: ask the table for what it kept.
     */
    statements
        = nr_php_objtable_get(NRPRG_SHARED(pgsql_statements), pgsql_conn);
  }

  return statements;
}

typedef struct {
  const char* key;
  size_t key_len;
  uint64_t used;
} nr_php_pgsql_least_used_t;

static void nr_php_pgsql_least_used_iterator(void* value,
                                             const char* key,
                                             size_t key_len,
                                             void* user_data) {
  const nr_php_pgsql_statement_t* statement = value;
  nr_php_pgsql_least_used_t* least = user_data;

  if ((NULL == least->key) || (statement->used < least->used)) {
    least->key = key;
    least->key_len = key_len;
    least->used = statement->used;
  }
}

/*
 * A connection that lives as long as a worker, and prepares statements with
 * names that are not used again, would otherwise hold them all. Making room
 * drops the statement used least recently: its execute is reported as an
 * unknown prepared statement, never with the SQL of another one.
 *
 * The limit is a number of statements, not a number of bytes, so a connection
 * that holds the maximum number of statements with very long SQL holds a lot of
 * memory. Looking for the statement used least recently visits every statement
 * of the connection, and happens only for a new name once the limit is
 * reached.
 */
static void nr_php_pgsql_statements_make_room(
    nr_php_pgsql_statements_t* statements) {
  nr_php_pgsql_least_used_t least = {NULL, 0, 0};
  char* key;

  if (nr_hashmap_count(statements->names) < NR_PHP_PGSQL_MAX_STATEMENTS) {
    return;
  }

  nr_hashmap_apply(statements->names, nr_php_pgsql_least_used_iterator, &least);
  if (NULL == least.key) {
    return;
  }

  /* The key belongs to the entry that is deleted: copy it first. */
  key = nr_strndup(least.key, least.key_len);
  nr_hashmap_delete(statements->names, key, least.key_len);
  nr_free(key);
}

void nr_php_pgsql_statement_save(const zval* pgsql_conn,
                                 const char* name,
                                 nr_string_len_t name_len,
                                 const char* sql,
                                 nr_string_len_t sql_len) {
  char buffer[NR_PHP_PGSQL_STATEMENT_KEY_BUFFER];
  char* heap_key;
  const char* key;
  size_t key_len;
  nr_php_pgsql_statement_t* statement;
  nr_php_pgsql_statements_t* statements;

  if ((NULL == sql) || (0 == sql_len)) {
    return;
  }

  statements = nr_php_pgsql_statements_find(pgsql_conn, 1);
  if (NULL == statements) {
    return;
  }

  key = nr_php_pgsql_statement_key(name, name_len, buffer, sizeof(buffer),
                                   &heap_key, &key_len);

  /* A name that is prepared again replaces its statement: no room is needed. */
  if (NULL == nr_hashmap_get(statements->names, key, key_len)) {
    nr_php_pgsql_statements_make_room(statements);
  }

  statement = nr_malloc(sizeof(nr_php_pgsql_statement_t));
  statement->sql = nr_strndup(sql, sql_len);
  statement->used = ++statements->clock;
  statement->txn_start = (name_len > 0) ? 0 : nr_txn_start_time(NRPRG(txn));
  nr_hashmap_update(statements->names, key, key_len, statement);
  nr_free(heap_key);
}

const char* nr_php_pgsql_statement_find(const zval* pgsql_conn,
                                        const char* name,
                                        nr_string_len_t name_len) {
  char buffer[NR_PHP_PGSQL_STATEMENT_KEY_BUFFER];
  char* heap_key;
  const char* key;
  size_t key_len;
  nr_php_pgsql_statement_t* statement;
  nr_php_pgsql_statements_t* statements;

  statements = nr_php_pgsql_statements_find(pgsql_conn, 0);
  if (NULL == statements) {
    return NULL;
  }

  key = nr_php_pgsql_statement_key(name, name_len, buffer, sizeof(buffer),
                                   &heap_key, &key_len);
  statement = nr_hashmap_get(statements->names, key, key_len);
  nr_free(heap_key);

  if (NULL == statement) {
    return NULL;
  }

  /*
   * The unnamed statement is only the one that was prepared in this
   * transaction. Within the transaction it can still be stale:
   * pg_query_params() replaces it on the server and pg_query() destroys it, and
   * the agent does not follow either, so the next pg_execute() of the unnamed
   * statement can run SQL other than the one that is saved.
   */
  if ((name_len <= 0)
      && (statement->txn_start != nr_txn_start_time(NRPRG(txn)))) {
    return NULL;
  }

  /*
   * The agent does not follow a statement that the application removes on the
   * server (DEALLOCATE, DISCARD ALL) or that a connection reset drops. The SQL
   * stays here, and a pg_execute() of that name fails on the server: the failed
   * call is named after the SQL that was saved.
   */
  statement->used = ++statements->clock;
  return statement->sql;
}
#endif
