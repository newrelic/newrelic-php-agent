/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles fibers instrumentation.
 */

#include "nr_mysqli_metadata.h"
#include "nr_mysqli_metadata_private.h"
#include "php_agent.h"
#include "php_newrelic.h"
#include "util_hashmap.h"
#include "util_memory.h"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#define COPY_FIELD(x) (dest->x = src->x)
#define COPY_HASHMAP(x) (dest->x = nr_hashmap_copy(NRTXNGLOBAL(x)))

static void nrf_txn_global_deep_copy(txn_globals_t* dest, txn_globals_t* src) {
  COPY_FIELD(execute_count);
  COPY_FIELD(generating_explain_plan);
  COPY_FIELD(curl_ignore_setopt);

  COPY_HASHMAP(guzzle_objs);
  COPY_HASHMAP(mysqli_queries);
  COPY_HASHMAP(pdo_link_options);
  COPY_HASHMAP(curl_metadata);
  COPY_HASHMAP(curl_multi_metadata);
  COPY_HASHMAP(prepared_statements);

  dest->mysqli_links = nr_mysqli_metadata_copy(NRTXNGLOBAL(mysqli_links));
}

#undef COPY_FIELD
#undef COPY_HASHMAP

txn_globals_t* nrf_fiber_init_txn_globals() {
  txn_globals_t* fiber_txn_globals = NULL;

  fiber_txn_globals = nr_malloc(sizeof(txn_globals_t));

  nrf_txn_global_deep_copy(fiber_txn_globals, &NRPRG(txn_globals));

  return fiber_txn_globals;
}

void free_fiber_globals(void* fiber_globals) {
  fiber_globals_t* f = (fiber_globals_t*)fiber_globals;
  nr_hashmap_destroy(&f->txn_globals->guzzle_objs);
  nr_hashmap_destroy(&f->txn_globals->mysqli_queries);
  nr_hashmap_destroy(&f->txn_globals->pdo_link_options);
  nr_hashmap_destroy(&f->txn_globals->curl_metadata);
  nr_hashmap_destroy(&f->txn_globals->curl_multi_metadata);
  nr_hashmap_destroy(&f->txn_globals->prepared_statements);
  nr_mysqli_metadata_destroy(&f->txn_globals->mysqli_links);
}

void nrf_fiber_init_global_hashmap() {
  if (NULL != NRPRG_SHARED(fiber_globals_map)) {
    NRPRG_SHARED(fiber_globals_map) = nr_hashmap_create(free_fiber_globals);
  }
}

void nrf_fiber_destroy_global_hashmap() {
  if (NULL != NRPRG_SHARED(fiber_globals_map)) {
    nr_hashmap_destroy(&NRPRG_SHARED(fiber_globals_map));
  }
}

#endif  // PHP 8.1+
