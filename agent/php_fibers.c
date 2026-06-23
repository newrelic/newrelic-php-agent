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
#include "php_agent.h"
#include "php_newrelic.h"
#include "php_zval.h"
#include "util_hashmap.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_stack.h"
#include "util_strings.h"
#include "util_time.h"
#include "zend_types.h"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#define COPY_BASIC(x) (dest->x = src->x)
#define COPY_POINTER(x) (dest->x = &src->x)
#define COPY_STRING(x) (dest->x = src->x ? nr_strdup(src->x) : NULL)
#define COPY_STACK(x, y) (dest->x = nr_stack_copy(&src->x, y))

static void* copy_elem_zval(void* elem) {
  if (NULL == elem) {
    return NULL;
  }
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

static void fiber_str_dtor(void* e, NRUNUSED void* d) {
  nr_free(e);
}

static void nrf_ctx_global_deep_copy(ctx_globals_t* dest, ctx_globals_t* src) {
  if (NULL == dest) {
    return;
  }

  COPY_STRING(doctrine_dql);

  dest->drupal_http_request_depth = 0;
  dest->php_cur_stack_depth = 0;  // PHP allocates a new stack for each fiber

  COPY_STRING(mysql_last_conn);
  COPY_STRING(pgsql_last_conn);

  COPY_BASIC(datastore_connections);

  COPY_BASIC(deprecated_capture_request_parameters);
  COPY_BASIC(check_cufa);

  COPY_BASIC(predis_commands);

  COPY_STACK(drupal_invoke_all_hooks, copy_elem_zval);
  COPY_STACK(drupal_invoke_all_states, copy_elem_ident);

  dest->drupal_http_request_segment = NULL;

  COPY_STACK(wordpress_tags, copy_elem_str);
  dest->wordpress_tags.dtor = fiber_str_dtor;
  COPY_STACK(wordpress_tag_states, copy_elem_ident);
  COPY_STACK(predis_ctxs, copy_elem_str);
}

#undef COPY_STACK
#undef COPY_STRING
#undef COPY_BASIC

ctx_globals_t* nrf_fiber_copy_ctx_globals(ctx_globals_t* src) {
  ctx_globals_t* fiber_ctx_globals = NULL;

  if (NULL == src) {
    return NULL;
  }

  fiber_ctx_globals = nr_malloc(sizeof(ctx_globals_t));

  nrf_ctx_global_deep_copy(fiber_ctx_globals, src);

  return fiber_ctx_globals;
}

void free_fiber_globals(void* fiber_globals) {
  fiber_globals_t* f = (fiber_globals_t*)fiber_globals;

  if (NULL == fiber_globals) {
    nrl_warning(NRL_AGENT, "Failed to free fiber globals, target is NULL");
    return;
  }

  if (NULL == f->ctx_globals) {
    nrl_warning(NRL_AGENT, "Failed to free fiber globals, ctx_globals is NULL");
    nr_free(f);
    return;
  }

  nr_free(f->ctx_globals->doctrine_dql);
  nr_free(f->ctx_globals->mysql_last_conn);
  nr_free(f->ctx_globals->pgsql_last_conn);
  nr_stack_destroy_fields(&f->ctx_globals->drupal_invoke_all_hooks);
  nr_stack_destroy_fields(&f->ctx_globals->drupal_invoke_all_states);
  nr_stack_destroy_fields(&f->ctx_globals->wordpress_tags);
  nr_stack_destroy_fields(&f->ctx_globals->wordpress_tag_states);
  nr_stack_destroy_fields(&f->ctx_globals->predis_ctxs);
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
    ctx_globals_t* src_ctx_globals,
    const char* key) {
  fiber_globals_t* fg = NULL;
  ctx_globals_t* cg = NULL;

  if (NULL == key || nr_strlen(key) < 1) {
    return NR_FAILURE;
  }

  if (NULL == fiber_globals_map) {
    return NR_FAILURE;
  }

  fg = nr_malloc(sizeof(fiber_globals_t));
  cg = nrf_fiber_copy_ctx_globals(src_ctx_globals);
  if (NULL == cg) {
    nr_free(fg);
    return NR_FAILURE;
  }

  fg->ctx_globals = cg;

  nr_hashmap_update(fiber_globals_map, key, nr_strlen(key), fg);

  return NR_SUCCESS;
}

nr_status_t nrf_remove_fiber_context_from_global_hashmap(
    nr_hashmap_t* fiber_globals_map,
    const char* key) {
  if (nr_strempty(key)) {
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

  if (NULL == fiber_global_ptr) {
    return NR_FAILURE;
  }

  if (NULL == key) {
    // a NULL key indicates we're in the MAIN php context rather than a fiber.
    // Set the fiber pointer to NULL to prevent using the fiber-specific
    // accessors.
    *fiber_global_ptr = NULL;
    return NR_SUCCESS;
  }

  if (NULL == fiber_globals_map) {
    *fiber_global_ptr = NULL;
    return NR_FAILURE;
  }

  fg = nr_hashmap_get(fiber_globals_map, key, nr_strlen(key));

  if (NULL == fg) {
    *fiber_global_ptr = NULL;
    return NR_FAILURE;
  }

  *fiber_global_ptr = fg;

  return NR_SUCCESS;
}

#endif  // PHP 8.1+
