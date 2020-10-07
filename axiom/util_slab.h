/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions related to simple slab allocation for homogeneous objects.
 */
#ifndef UTIL_SLAB_HDR
#define UTIL_SLAB_HDR

#include <stdbool.h>
#include <stddef.h>

typedef struct _nr_slab_t nr_slab_t;

/*
 * Purpose : Create a slab allocator for homogeneous objects.
 *
 * Params  : 1. The size of each object.
 *           2. The default page size, or 0 to use a value calculated based on
 *              the system page size and the object size.
 *
 * Returns : A slab allocator, or NULL if the object size is 0.
 *
 * Notes   : 1. Objects will be aligned on 16 byte boundaries, so there's
 *              little value in providing a value below 16.
 *           2. If the page size isn't a multiple of the system page size, it
 *              will be rounded up to the next multiple. If it is below the
 *              system page size, it will be raised to that page size.
 *
 * Warning : The slab allocator returned by this function is extremely dumb,
 *           and does not support basic features such as freeing the memory
 *           returned by nr_slab_next() or destructors. This is intentional: if
 *           you need these features, this is not for you.
 */
extern nr_slab_t* nr_slab_create(size_t object_size, size_t page_size);

/*
 * Purpose : Destroy a slab allocator.
 *
 * Params  : 1. A pointer to the slab allocator to destroy.
 */
extern void nr_slab_destroy(nr_slab_t** slab_ptr);

/*
 * Purpose : Return the next available chunk of memory in the slab allocator.
 *
 * Params  : 1. The slab allocator.
 *
 * Returns : A chunk of memory, or NULL on error.
 */
extern void* nr_slab_next(nr_slab_t* slab);

/*
 * Purpose : Release an object allocated by a slab allocator to the slab free
 *           list.
 *
 * Params  : 1. The slab allocator.
 *           2. The object pointer to release.
 *
 * Returns : True if the object was released; false otherwise.
 *
 * Warning : This function does not check if the object was actually returned
 *           by a previous nr_slab_next() call: that is the responsibility of
 *           the caller.
 */
extern bool nr_slab_release(nr_slab_t* slab, void* obj);

/*
 * Purpose : Return the total number of objects returned.
 *
 * Params  : 1. The slab allocator.
 *
 * Returns : The number of objects returned (this is the number of times
 *           nr_slab_next was called succesfully).
 */
extern size_t nr_slab_count(const nr_slab_t* slab);

#endif /* UTIL_SLAB_HDR */
