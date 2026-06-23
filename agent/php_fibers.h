/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NEWRELIC_PHP_AGENT_PHP_FIBERS_H
#define NEWRELIC_PHP_AGENT_PHP_FIBERS_H

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#include "php_newrelic.h"

/*
 * Purpose : Allocate and deep-copy the given context globals into a new
 *           ctx_globals_t suitable for use by a fiber.
 *
 * Params  : 1. A pointer to the source ctx_globals_t to copy from. Must be
 *              non-NULL. The source is not modified and remains owned by
 *              the caller; typically this is &NRPRG(ctx).
 *
 * Returns : A pointer to a newly allocated ctx_globals_t. Ownership of the
 *           returned struct is transferred to the caller, which is responsible
 *           for freeing it.
 */
extern ctx_globals_t* nrf_fiber_copy_ctx_globals(ctx_globals_t* src);

/*
 * Purpose : Free a fiber_globals_t and all owned resources held by its
 *           contained  ctx_globals
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
 *           If *fiber_globals_map is NULL, a new hashmap is allocated with
 *           free_fiber_globals as its destructor (so that removed or replaced
 *           entries are fully cleaned up) and stored back into
 *           *fiber_globals_map. If *fiber_globals_map already refers to a
 *           hashmap, it is left unchanged and NR_FAILURE is returned so the
 *           existing hashmap is not overwritten (and leaked).
 *
 * Params  : 1. Address of the fiber globals hashmap pointer to initialize;
 *              typically &NRPRG(fiber_globals_map). Must be non-NULL, and
 *              *fiber_globals_map must be NULL.
 *
 * Returns : NR_SUCCESS if a new hashmap was allocated and stored;
 *           NR_FAILURE if fiber_globals_map is NULL or *fiber_globals_map
 *           was already non-NULL.
 */
extern nr_status_t nrf_fiber_init_global_hashmap(
    nr_hashmap_t** fiber_globals_map);

/*
 * Purpose : Destroy the per-request fiber globals hashmap, freeing every
 *           fiber_globals_t snapshot it contains via free_fiber_globals, and
 *           set *fiber_globals_map to NULL.
 *
 * Params  : 1. Address of the fiber globals hashmap pointer to destroy;
 *              typically &NRPRG(fiber_globals_map). Safe to call when
 *              *fiber_globals_map is NULL.
 *
 * Returns : NR_SUCCESS if the operation was performed (the parameter was
 *           non-NULL), NR_FAILURE otherwise.
 */
extern nr_status_t nrf_fiber_destroy_global_hashmap(
    nr_hashmap_t** fiber_globals_map);

/*
 * Purpose : Deep-copy the given  context globals into a new fiber_globals_t
 *           snapshot and store it in the given fiber globals hashmap
 *           under the given key. Used when a fiber is suspended so
 *           its globals can be restored on resume.
 *
 * Params  : 1. The fiber globals hashmap into which the snapshot should be
 *              stored; typically NRPRG(fiber_globals_map). Must be non-NULL
 *              and previously initialized via nrf_fiber_init_global_hashmap.
 *           2. A pointer to the source ctx_globals_t to snapshot from;
 *              typically &NRPRG(ctx). The source is not modified and remains
 *              owned by the caller.
 *           3. The key identifying the fiber whose globals are being saved.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid or
 *           the fiber globals hashmap is NULL.
 */
extern nr_status_t nrf_add_fiber_context_to_global_hashmap(
    nr_hashmap_t* fiber_globals_map,
    ctx_globals_t* src_ctx_globals,
    const char* key);

/*
 * Purpose : Remove the fiber globals snapshot associated with the given key
 *           from the given fiber globals hashmap, freeing the snapshot.
 *
 * Params  : 1. The fiber globals hashmap from which the snapshot should be
 *              removed; typically NRPRG(fiber_globals_map). Must be non-NULL.
 *           2. The key identifying the fiber whose globals should be removed.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid, the
 *           hashmap is NULL, or no entry exists for the key.
 */
extern nr_status_t nrf_remove_fiber_context_from_global_hashmap(
    nr_hashmap_t* fiber_globals_map,
    const char* key);

/*
 * Purpose : Switch the active fiber globals pointer to the snapshot stored
 *           in the given fiber globals hashmap under the given key. Used when
 *           a fiber is resumed so that subsequent instrumentation operates
 *           against that fiber's saved context state.
 *
 *           On success, the snapshot retrieved from the hashmap is written
 *           into the caller-supplied destination pointer; typically this is
 *           NRPRG(fiber_globals).
 *
 * Params  : 1. The fiber globals hashmap to look up the snapshot in;
 *              typically NRPRG(fiber_globals_map). Must be non-NULL.
 *           2. The destination where the active snapshot pointer should be
 *              written; typically &NRPRG(fiber_globals). The snapshot itself
 *              remains owned by the hashmap and must not be freed by the
 *              caller.
 *           3. The key identifying the fiber whose globals should become
 *              active.
 *
 * Returns : NR_SUCCESS on success, or NR_FAILURE if the key is invalid, the
 *           hashmap is NULL, or no snapshot is stored under the given key.
 */
extern nr_status_t nrf_fiber_switch_global_context(
    nr_hashmap_t* fiber_globals_map,
    fiber_globals_t** fiber_global_ptr,
    const char* key);

#endif  // PHP 8.1+
#endif  // NEWRELIC_PHP_AGENT_PHP_FIBERS_H
