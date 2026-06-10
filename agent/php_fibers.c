/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles fibers instrumentation.
 */

#include "nr_datastore_instance.h"
#include "php_includes.h"
#include "php_compat.h"
#include "php_fibers.h"
#include "nr_mysqli_metadata.h"
#include "nr_mysqli_metadata_private.h"
#include "php_agent.h"
#include "php_newrelic.h"
#include "php_zval.h"
#include "util_hashmap.h"
#include "util_memory.h"
#include "util_stack.h"
#include "util_strings.h"
#include "util_time.h"
#include "zend_types.h"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#define COPY_BASIC(x) (dest->x = src->x)
#define COPY_STRING(x) (dest->x = nr_strdup(src->x))
#define COPY_HASHMAP(x, y) (dest->x = nr_hashmap_copy(src->x, y))
#define COPY_STACK(x, y) (dest->x = nr_stack_copy(&src->x, y))

static void* copy_elem_zval(void* elem) {
  zval* copy = nr_php_zval_alloc();
  ZVAL_DUP(copy, elem);
  return copy;
}

static void* copy_elem_ident(void* elem) {
  return elem;
}

static void* copy_elem_str(void* elem) {
  return nr_strdup((char*)elem);
}

static void* copy_elem_time(void* elem) {
  nrtime_t* t = NULL;
  nrtime_t* src = (nrtime_t*)elem;
  t = (nrtime_t*)nr_malloc(sizeof(nrtime_t));
  *t = *src;
  return t;
}

static void* copy_datastore_instance(void* elem) {
  nr_datastore_instance_t* d = NULL;
  nr_datastore_instance_t* src = (nr_datastore_instance_t*)elem;
  d = nr_malloc(sizeof(nr_datastore_instance_t));
  d->host = nr_strdup(src->host);
  d->port_path_or_id = nr_strdup(src->port_path_or_id);
  d->database_name = nr_strdup(src->database_name);
  return d;
}

static void nrf_txn_global_deep_copy(txn_globals_t* dest, txn_globals_t* src) {
  COPY_BASIC(execute_count);
  COPY_BASIC(generating_explain_plan);
  COPY_BASIC(curl_ignore_setopt);

  COPY_BASIC(guzzle_objs);
  COPY_HASHMAP(mysqli_queries, copy_elem_zval);
  COPY_HASHMAP(pdo_link_options, copy_elem_zval);
  COPY_BASIC(curl_metadata);
  COPY_BASIC(curl_multi_metadata);
  COPY_HASHMAP(prepared_statements, copy_elem_str);

  dest->mysqli_links = nr_mysqli_metadata_copy(src->mysqli_links);
}

static void nrf_ctx_global_deep_copy(ctx_globals_t* dest, ctx_globals_t* src) {
  COPY_STRING(doctrine_dql);

  COPY_BASIC(drupal_http_request_depth);
  COPY_BASIC(php_cur_stack_depth);

  COPY_BASIC(cufa_callback);

  COPY_STRING(mysql_last_conn);
  COPY_STRING(pgsql_last_conn);

  COPY_HASHMAP(datastore_connections, copy_datastore_instance);

  COPY_BASIC(deprecated_capture_request_parameters);
  COPY_BASIC(error_group_user_callback);
  COPY_BASIC(check_cufa);

  COPY_HASHMAP(predis_commands, copy_elem_time);

  COPY_STACK(drupal_invoke_all_hooks, copy_elem_zval);
  COPY_STACK(drupal_invoke_all_states, copy_elem_ident);

  dest->drupal_http_request_segment = NULL;

  COPY_STACK(wordpress_tags, copy_elem_str);
  COPY_STACK(wordpress_tag_states, copy_elem_ident);
  COPY_STACK(predis_ctxs, copy_elem_str);
}

#undef COPY_STACK
#undef COPY_HASHMAP
#undef COPY_STRING
#undef COPY_BASIC

txn_globals_t* nrf_fiber_copy_txn_globals(txn_globals_t* src) {
  txn_globals_t* fiber_txn_globals = NULL;

  fiber_txn_globals = nr_malloc(sizeof(txn_globals_t));

  nrf_txn_global_deep_copy(fiber_txn_globals, src);

  return fiber_txn_globals;
}

ctx_globals_t* nrf_fiber_copy_ctx_globals(ctx_globals_t* src) {
  ctx_globals_t* fiber_ctx_globals = NULL;

  fiber_ctx_globals = nr_malloc(sizeof(ctx_globals_t));

  nrf_ctx_global_deep_copy(fiber_ctx_globals, src);

  return fiber_ctx_globals;
}

void free_fiber_globals(void* fiber_globals) {
  fiber_globals_t* f = (fiber_globals_t*)fiber_globals;
  nr_hashmap_destroy(&f->txn_globals->mysqli_queries);
  nr_hashmap_destroy(&f->txn_globals->pdo_link_options);
  nr_hashmap_destroy(&f->txn_globals->prepared_statements);
  nr_mysqli_metadata_destroy(&f->txn_globals->mysqli_links);

  nr_free(f->ctx_globals->doctrine_dql);
  nr_free(f->ctx_globals->mysql_last_conn);
  nr_free(f->ctx_globals->pgsql_last_conn);
  nr_hashmap_destroy(&f->ctx_globals->datastore_connections);
  nr_hashmap_destroy(&f->ctx_globals->predis_commands);
  nr_stack_destroy_fields(&f->ctx_globals->drupal_invoke_all_hooks);
  nr_stack_destroy_fields(&f->ctx_globals->drupal_invoke_all_states);
  nr_stack_destroy_fields(&f->ctx_globals->wordpress_tags);
  nr_stack_destroy_fields(&f->ctx_globals->wordpress_tag_states);
  nr_stack_destroy_fields(&f->ctx_globals->predis_ctxs);
  nr_free(f->txn_globals);
  nr_free(f->ctx_globals);
  nr_free(f);
}

nr_status_t nrf_fiber_init_global_hashmap(nr_hashmap_t** fiber_globals_map) {
  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }
  if (NULL == *fiber_globals_map) {
    *fiber_globals_map = nr_hashmap_create(free_fiber_globals);
  }
  return NR_SUCCESS;
}

nr_status_t nrf_fiber_destroy_global_hashmap(nr_hashmap_t** fiber_globals_map) {
  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }
  if (NULL != *fiber_globals_map) {
    nr_hashmap_destroy(fiber_globals_map);
  }
  return NR_SUCCESS;
}

nr_status_t nrf_add_fiber_context_to_global_hashmap(
    nr_hashmap_t* fiber_globals_map,
    txn_globals_t* src_txn_globals,
    ctx_globals_t* src_ctx_globals,
    const char* key) {
  fiber_globals_t* fg = NULL;
  txn_globals_t* tg = NULL;
  ctx_globals_t* cg = NULL;

  if (NULL == key || nr_strlen(key) < 1) {
    return NR_FAILURE;
  }

  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }

  fg = nr_malloc(sizeof(fiber_globals_t));
  tg = nrf_fiber_copy_txn_globals(src_txn_globals);
  cg = nrf_fiber_copy_ctx_globals(src_ctx_globals);

  fg->txn_globals = tg;
  fg->ctx_globals = cg;

  nr_hashmap_update(fiber_globals_map, key, nr_strlen(key), fg);

  return NR_SUCCESS;
}

nr_status_t nrf_remove_fiber_context_from_global_hashmap(
    nr_hashmap_t* fiber_globals_map,
    const char* key) {
  if (NULL == key || nr_strlen(key) < 1) {
    return NR_FAILURE;
  }

  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }

  return nr_hashmap_delete(fiber_globals_map, key, nr_strlen(key));
}

nr_status_t nrf_fiber_switch_global_context(nr_hashmap_t* fiber_globals_map,
                                            fiber_globals_t** fiber_global_ptr,
                                            const char* key) {
  fiber_globals_t* fg = NULL;
  if (NULL == key) {
    // a NULL key indicates we're in the MAIN php context rather than a fiber.
    // Set the fiber pointer to NULL to prevent using the fiber-specific
    // accessors.
    *fiber_global_ptr = NULL;
    return NR_SUCCESS;
  }

  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }

  if (NULL == fiber_global_ptr) {
    return NR_FAILURE;
  }

  fg = nr_hashmap_get(fiber_globals_map, key, nr_strlen(key));

  if (NULL == fg) {
    return NR_FAILURE;
  }

  *fiber_global_ptr = fg;

  return NR_SUCCESS;
}

#endif  // PHP 8.1+
