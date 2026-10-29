/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_globals.h"
#include "php_oci.h"
#include "util_hashmap.h"
#include "util_memory.h"
#include "util_vector.h"

typedef struct {
  uint64_t id;
  char* sql;
} nr_php_oci_statement_t;

static void nr_php_oci_statement_destroy(void* value) {
  nr_php_oci_statement_t* statement = value;

  if (NULL == statement) {
    return;
  }

  nr_free(statement->sql);
  nr_free(statement);
}

void nr_php_oci_statements_create(void) {
  NRPRG_SHARED(oci_statements)
      = nr_hashmap_create(nr_php_oci_statement_destroy);
}

void nr_php_oci_statements_destroy(void) {
  nr_hashmap_destroy(&NRPRG_SHARED(oci_statements));
}

static void nr_php_oci_statements_collect_freed(void* value,
                                                const char* key NRUNUSED,
                                                size_t key_len NRUNUSED,
                                                void* user_data) {
  const nr_php_oci_statement_t* statement = value;
  nr_vector_t* freed = user_data;

  /*
   * The resource list drops a resource when its last reference is released,
   * and its id is not given to another resource.
   */
  if (!zend_hash_index_exists(&EG(regular_list), (zend_ulong)statement->id)) {
    nr_vector_push_back(freed, (void*)(uintptr_t)statement->id);
  }
}

/*
 * The entries of freed statements are dropped when a new statement is parsed,
 * not when the statement is freed. The SQL of a statement that was freed stays
 * in memory until then, or until the request ends; it is bounded by the number
 * of statements that were parsed since the last parse.
 */
static void nr_php_oci_statements_sweep(void) {
  size_t i;
  nr_vector_t* freed;

  if (0 == nr_hashmap_count(NRPRG_SHARED(oci_statements))) {
    return;
  }

  freed = nr_vector_create(8, NULL, NULL);

  nr_hashmap_apply(NRPRG_SHARED(oci_statements),
                   nr_php_oci_statements_collect_freed, freed);

  for (i = 0; i < nr_vector_size(freed); i++) {
    nr_hashmap_index_delete(NRPRG_SHARED(oci_statements),
                            (uint64_t)(uintptr_t)nr_vector_get(freed, i));
  }

  nr_vector_destroy(&freed);
}

void nr_php_oci_statement_sql_set(const zval* stmt,
                                  const char* sql,
                                  nr_string_len_t sql_len) {
  nr_php_oci_statement_t* statement;

  if ((NULL == NRPRG_SHARED(oci_statements))
      || !nr_php_is_zval_valid_resource(stmt)) {
    return;
  }

  nr_php_oci_statements_sweep();

  if ((NULL == sql) || (sql_len <= 0)) {
    nr_hashmap_index_delete(NRPRG_SHARED(oci_statements),
                            (uint64_t)nr_php_zval_resource_id(stmt));
    return;
  }

  statement = nr_malloc(sizeof(nr_php_oci_statement_t));
  statement->id = (uint64_t)nr_php_zval_resource_id(stmt);
  statement->sql = nr_strndup(sql, sql_len);
  nr_hashmap_index_update(NRPRG_SHARED(oci_statements), statement->id,
                          statement);
}

const char* nr_php_oci_statement_sql_get(const zval* stmt) {
  const nr_php_oci_statement_t* statement;

  if ((NULL == NRPRG_SHARED(oci_statements))
      || !nr_php_is_zval_valid_resource(stmt)) {
    return NULL;
  }

  statement = nr_hashmap_index_get(NRPRG_SHARED(oci_statements),
                                   (uint64_t)nr_php_zval_resource_id(stmt));

  return statement ? statement->sql : NULL;
}

size_t nr_php_oci_statements_count(void) {
  return NRPRG_SHARED(oci_statements)
             ? nr_hashmap_count(NRPRG_SHARED(oci_statements))
             : 0;
}
