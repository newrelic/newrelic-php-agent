/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Eviction and the owner check for the per-thread maps. The get-or-create
 * functions call the check, which can evict, so like them these live outside
 * nr_app.c, which depends on the daemon connection code in nr_agent.c.
 */

#include "nr_axiom.h"

#include <stdint.h>

#include "nr_app.h"
#include "util_hashmap.h"
#include "util_threads.h"

void nr_app_tid_maps_evict(nrapp_t* app, uint64_t tid) {
  if (NULL == app) {
    return;
  }
  nr_hashmap_index_delete(app->harvest_map, tid);
  nr_hashmap_index_delete(app->rnd_map, tid);
  nr_hashmap_index_delete(app->composer_map, tid);
  nr_hashmap_index_delete(app->tid_owner_map, tid);
}

/*
 * Each thread gets an incarnation on its first call into the per-thread maps.
 * Unlike a tid, an incarnation is never reused: it comes from a process-wide
 * counter starting at 1, and the thread-local starts at 0, meaning not yet
 * assigned, in every new thread. app_lock doesn't cover the counter, since it
 * is shared by every app, so it has its own mutex. That mutex is taken once
 * per thread, and nothing else is acquired while it is held, so it can't take
 * part in a lock-order cycle.
 */
static nrthread_mutex_t nr_app_incarnation_mutex = NRTHREAD_MUTEX_INITIALIZER;
static uint64_t nr_app_next_incarnation = 1;
static nrt_thread_local uint64_t nr_app_thread_incarnation = 0;

static uint64_t nr_app_get_thread_incarnation(void) {
  if (0 == nr_app_thread_incarnation) {
    nrt_mutex_lock(&nr_app_incarnation_mutex);
    nr_app_thread_incarnation = nr_app_next_incarnation++;
    nrt_mutex_unlock(&nr_app_incarnation_mutex);
  }
  return nr_app_thread_incarnation;
}

void nr_app_tid_maps_check_owner(nrapp_t* app, uint64_t tid) {
  uint64_t incarnation;
  void* owner;
  void* self;

  if (NULL == app || NULL == app->tid_owner_map) {
    return;
  }

  /* Incarnations start at 1, so a NULL value means no owner is recorded. */
  incarnation = nr_app_get_thread_incarnation();
  self = (void*)(uintptr_t)incarnation;
  owner = nr_hashmap_index_get(app->tid_owner_map, tid);
  if (owner == self) {
    return;
  }

  /*
   * A different owner is an earlier thread that had this tid and exited
   * without evicting its entries. With no owner, no get-or-create has run for
   * this tid since its entries were last evicted, so there is nothing to
   * evict.
   */
  if (NULL != owner) {
    nr_app_tid_maps_evict(app, tid);
  }
  nr_hashmap_index_update(app->tid_owner_map, tid, self);
}
