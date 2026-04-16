/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles fibers instrumentation.
 */

#include "nr_mysqli_metadata_private.h"
#include "php_agent.h"
#include "php_newrelic.h"
#include "util_hashmap.h"
#include "util_memory.h"

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#define COPY_FIELD(x) (dest->x = src->x)

static void nrf_txn_global_deep_copy(txn_globals_t* dest, txn_globals_t* src) {
  COPY_FIELD(execute_count);
  COPY_FIELD(generating_explain_plan);
  COPY_FIELD(curl_ignore_setopt);

  dest->guzzle_objs = nr_hashmap_copy(NRTXNGLOBAL(guzzle_objs));
  dest->mysqli_queries = nr_hashmap_copy(NRTXNGLOBAL(mysqli_queries));
  dest->pdo_link_options = nr_hashmap_copy(NRTXNGLOBAL(pdo_link_options));
  dest->curl_metadata = nr_hashmap_copy(NRTXNGLOBAL(curl_metadata));
  dest->curl_multi_metadata = nr_hashmap_copy(NRTXNGLOBAL(curl_multi_metadata));
  dest->prepared_statements = nr_hashmap_copy(NRTXNGLOBAL(prepared_statements));

  dest->mysqli_links = nr_mysqli_metadata_copy(NRTXNGLOBAL(mysqli_links));
}

txn_globals_t* nrf_fiber_init_txn_globals() {
  txn_globals_t* fiber_globals = NULL;

  fiber_globals = nr_malloc(sizeof(txn_globals_t));

  nrf_txn_global_deep_copy(fiber_globals, &NRPRG(txn_globals));

  return fiber_globals;
}

#undef COPY_FIELD

#endif  // PHP 8.1+
