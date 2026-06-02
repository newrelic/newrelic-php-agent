/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NEWRELIC_PHP_AGENT_PHP_FIBERS_H
#define NEWRELIC_PHP_AGENT_PHP_FIBERS_H

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#include "php_newrelic.h"

/*
 * Purpose : Allocate and deep-copy the given transaction globals into a new
 *           txn_globals_t suitable for use by a fiber.
 *
 * Params  : 1. A pointer to the source txn_globals_t to copy from. Must be
 *              non-NULL. The source is not modified and remains owned by
 *              the caller; typically this is &NRPRG(txn_globals).
 *
 * Returns : A pointer to a newly allocated txn_globals_t. Ownership of the
 *           returned struct (and its contained hashmaps and metadata) is
 *           transferred to the caller, which is responsible for freeing it.
 */
extern txn_globals_t* nrf_fiber_copy_txn_globals(txn_globals_t* src);

/*
 * Purpose : Allocate and deep-copy the given context globals into a new
 *           ctx_globals_t suitable for use by a fiber.
 *
 * Params  : 1. A pointer to the source ctx_globals_t to copy from. Must be
 *              non-NULL. The source is not modified and remains owned by
 *              the caller; typically this is &NRPRG(ctx).
 *
 * Returns : A pointer to a newly allocated ctx_globals_t. Ownership of the
 *           returned struct (and its contained strings, hashmaps, and stacks)
 *           is transferred to the caller, which is responsible for freeing it.
 */
extern ctx_globals_t* nrf_fiber_copy_ctx_globals(ctx_globals_t* src);

/*
 * Purpose : Free a fiber_globals_t and all owned resources held by its
 *           contained txn_globals and ctx_globals (hashmaps, mysqli metadata,
 *           strings, and stacks).
 *
 *           Intended to be used as the destructor callback registered with
 *           the fiber globals hashmap.
 *
 * Params  : 1. A pointer to a fiber_globals_t, passed as void* to satisfy
 *              the hashmap destructor signature.
 */
extern void free_fiber_globals(void* fiber_globals);

/*
 * Purpose : Initialize the per-request hashmap that maps fiber context keys
 *           to their saved fiber_globals_t snapshots.
 *
 *           The hashmap is created with free_fiber_globals as its destructor
 *           so that removed or replaced entries are fully cleaned up.
 */
extern void nrf_fiber_init_global_hashmap(void);

/*
 * Purpose : Destroy the per-request fiber globals hashmap, freeing every
 *           fiber_globals_t snapshot it contains via free_fiber_globals.
 */
extern void nrf_fiber_destroy_global_hashmap(void);

/*
 * Purpose : Snapshot the current transaction and context globals and store
 *           them in the fiber globals hashmap under the given key. Used when
 *           a fiber is suspended so its globals can be restored on resume.
 *
 * Params  : 1. The key identifying the fiber whose globals are being saved.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid or
 *           the fiber globals hashmap has not been initialized.
 */
extern nr_status_t nrf_add_fiber_context_to_global_hashmap(const char* key);

/*
 * Purpose : Remove the fiber globals snapshot associated with the given key
 *           from the fiber globals hashmap, freeing the snapshot.
 *
 * Params  : 1. The key identifying the fiber whose globals should be removed.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid, the
 *           hashmap has not been initialized, or no entry exists for the key.
 */
extern nr_status_t nrf_remove_fiber_context_from_global_hashmap(
    const char* key);

/*
 * Purpose : Switch the active fiber globals pointer to the snapshot stored
 *           under the given key. Used when a fiber is resumed so that
 *           subsequent instrumentation operates against that fiber's saved
 *           transaction and context state.
 *
 * Params  : 1. The key identifying the fiber whose globals should become
 *              active.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid, the
 *           hashmap has not been initialized, or no snapshot is stored under
 *           the given key.
 */
extern nr_status_t nrf_fiber_switch_global_context(const char* key);

#endif  // PHP 8.1+
#endif  // NEWRELIC_PHP_AGENT_PHP_FIBERS_H
