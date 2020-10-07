/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "util_slab.h"

#include <unistd.h>

#include "util_logging.h"
#include "util_memory.h"
#include "util_slab_private.h"

static nr_slab_page_t* nr_slab_page_create(size_t page_size,
                                           nr_slab_page_t* prev) {
  nr_slab_page_t* page;

  /*
   * Traditionally, one would implement this kind of allocator on top of
   * mmap(). In practice, though, the libc on our supported operating systems
   * will simply use mmap() inside malloc() anyway past a certain threshold,
   * which is generally going to be about 128kB. Rather than implementing the
   * somewhat arcane set of preprocessor magic required to use mmap()
   * effectively on a platform-by-platform basis, we'll just trust malloc() to
   * do the right thing.
   *
   * Plus, the whole point here is that we're not going to allocate or free
   * much from libc, so even if malloc() is slow, it won't cost us much.
   *
   * Technically, we're going to use nr_zalloc() to zero the page. This matches
   * the behaviour of mmap().
   */
  page = nr_zalloc(page_size);

  if (nrunlikely(NULL == page)) {
    return NULL;
  }

  *page = (nr_slab_page_t){
      .capacity = page_size - sizeof(nr_slab_page_t),
      .used = 0,
      .prev = prev,
  };

  return page;
}

nr_slab_t* nr_slab_create(size_t object_size, size_t page_size) {
  nr_slab_t* slab;
  long sys_page_size;

  if (nrunlikely(0 == object_size)) {
    return NULL;
  }

  slab = nr_malloc(sizeof(nr_slab_t));

  /*
   * Calculate the aligned size of the object.
   *
   * All architectures we support (and most we don't, but feasibly could) align
   * on 16 byte boundaries, so let's go with that. If we ever support something
   * exotic, we should figure out how to detect this at compile time.
   */
  slab->object_size
      = (object_size % 16 != 0) ? (16 * ((object_size / 16) + 1)) : object_size;

  /*
   * Grab the system page size so we can calculate the actual page size we're
   * going to use.
   */
  sys_page_size = sysconf(_SC_PAGESIZE);
  if (nrunlikely(sys_page_size <= 0)) {
    /*
     * In practice, everything we support uses pages <= 4k by default, so that's
     * a safe default.
     */
    sys_page_size = 4096;
  }

  /*
   * Calculate the initial page size.
   */
  if (0 == page_size) {
    /*
     * If 0 was given, then let's ensure the initial page size is large enough
     * for at least four objects per page. (Given the system page size
     * minimum, in practice, for pretty much any data structure we're
     * interested in it'll be much more than four.)
     */
    slab->page_size
        = sys_page_size * (((slab->object_size * 4) / sys_page_size) + 1);
  } else {
    // Otherwise, let's round up to the next multiple of the system page size.
    slab->page_size
        = sys_page_size
          * ((page_size % sys_page_size ? 1 : 0) + (page_size / sys_page_size));
  }

  /*
   * Basic sanity check to ensure we don't end up in a position where we can't
   * allocate objects. Another option here would be to increase the page size
   * until the objects would fit, but this scenario occurring at all likely
   * indicates a logic error in invoking this function.
   */
  if (nrunlikely((slab->object_size + sizeof(nr_slab_page_t))
                 > slab->page_size)) {
    nr_free(slab);
    return NULL;
  }

  /*
   * Create the first page.
   */
  slab->head = nr_slab_page_create(slab->page_size, NULL);

  /*
   * Set up the free list. The default 128 capacity was cargo culted from the
   * original implementation in the PHP agent.
   */
  nr_vector_init(&slab->free_list, 128, NULL, NULL);

  slab->count = 0;

  return slab;
}

void nr_slab_destroy(nr_slab_t** slab_ptr) {
  nr_slab_t* slab;
  nr_slab_page_t* head;

  if (nrunlikely(NULL == slab_ptr || NULL == *slab_ptr)) {
    return;
  }

  slab = *slab_ptr;
  nr_vector_deinit(&slab->free_list);

  /*
   * Actually destroying the slab allocator is easy: we just free each page
   * until we have no more pages.
   */
  head = slab->head;
  while (head) {
    nr_slab_page_t* prev = head->prev;

    nr_free(head);
    head = prev;
  }

  nr_realfree((void**)slab_ptr);
}

void* nr_slab_next(nr_slab_t* slab) {
  void* ptr;

  if (nrunlikely(NULL == slab || NULL == slab->head)) {
    return NULL;
  }

  /*
   * Check if we have any available objects in the free list.
   */
  if (nr_vector_size(&slab->free_list) > 0) {
    if (nrlikely(nr_vector_pop_back(&slab->free_list, &ptr))) {
      slab->count += 1;
      return ptr;
    }

    /*
     * Log the (very unexpected) error, but then fall through to go back to
     * the normal allocation logic below.
     *
     * This log message probably indicates memory corruption.
     */
    nrl_error(NRL_AGENT, "unable to pop an item off a non-empty free list");
  }

  /*
   * Check if the current page is full. If so, allocate a new page.
   */
  if ((slab->head->capacity - slab->head->used) < slab->object_size) {
    nr_slab_page_t* new_page;

    /*
     * Generally speaking, we want to increase the page size if it's small.
     * The 4 MB limit here is _totally_ arbitrary.
     */
    if (slab->page_size <= (4 * 1024 * 1024)) {
      slab->page_size *= 2;
    }

    new_page = nr_slab_page_create(slab->page_size, slab->head);
    if (nrunlikely(NULL == new_page)) {
      return NULL;
    }

    slab->head = new_page;
  }

  ptr = &slab->head->data[slab->head->used];
  slab->head->used += slab->object_size;

  slab->count += 1;
  return ptr;
}

bool nr_slab_release(nr_slab_t* slab, void* obj) {
  if (nrunlikely(NULL == slab || NULL == obj)) {
    return false;
  }

  /*
   * Although not explicitly documented, the slab allocator zeroes out pages
   * before handing out objects, so we'll zero the object before putting it
   * into the free list to keep the same behaviour. We will use memset()
   * directly here to gain the benefit of any compiler or libc optimisation:
   * we do not need the defensive checks in nr_memset(), as they have already
   * occurred either immediately above or when the slab was created.
   */
  memset(obj, 0, slab->object_size);

  // Actually insert the object into the free list.
  return nr_vector_push_back(&slab->free_list, obj);
}

size_t nr_slab_count(const nr_slab_t* slab) {
  if (slab) {
    return slab->count;
  } else {
    return 0;
  }
}
