/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A min-max heap, which can then be used to implement a simple double ended
 * priority queue.
 */
#ifndef UTIL_MINMAX_HEAP_HDR
#define UTIL_MINMAX_HEAP_HDR

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/*
 * The opaque min-max heap type.
 */
typedef struct _nr_minmax_heap_t nr_minmax_heap_t;

/*
 * Type declaration for comparison functions.
 *
 * Expected return values:
 *    a < b:  -1 (or another value less than 0)
 *    a == b: 0
 *    a > b:  1 (or another value greater than 0)
 */
typedef int (*nr_minmax_heap_cmp_t)(const void* a,
                                    const void* b,
                                    void* userdata);

/*
 * Type declaration for destructors.
 */
typedef void (*nr_minmax_heap_dtor_t)(void* value, void* userdata);

/*
 * Type declaration for iterators.
 */
typedef bool (*nr_minmax_heap_iter_t)(const void* value, void* userdata);

/*
 * Purpose : Create a min-max heap.
 *
 * Params  : 1. The maximum number of elements. If 0, an unbounded heap will be
 *              created. If 1 or negative, an error will be returned.
 *           2. The comparison function to use when comparing two values. This
 *              function is required.
 *           3. The userdata to be given to the comparison function when
 *              invoked. This may be NULL.
 *           4. The destructor to invoke when a value is either culled from a
 *              bounded heap or when the heap is destroyed. If NULL, no
 *              destructor will be invoked.
 *           5. The userdata to be given to the destructor when invoked. This
 *              may be NULL.
 *
 * Returns : A newly allocated min-max heap, or NULL on error.
 */
extern nr_minmax_heap_t* nr_minmax_heap_create(ssize_t bound,
                                               nr_minmax_heap_cmp_t comparator,
                                               void* comparator_userdata,
                                               nr_minmax_heap_dtor_t destructor,
                                               void* destructor_userdata);

/*
 * Purpose : Change destructor and destructor userdata for a min-max heap.
 *
 * Params  : 1. The heap.
 *           2. The destructor to invoke when a value is either culled from a
 *              bounded heap or when the heap is destroyed. If NULL, no
 *              destructor will be invoked.
 *           3. The userdata to be given to the destructor when invoked. This
 *              may be NULL.
 */
extern void nr_minmax_heap_set_destructor(nr_minmax_heap_t* heap,
                                          nr_minmax_heap_dtor_t destructor,
                                          void* destructor_userdata);
/*
 * Purpose : Destroy a min-max heap.
 *
 * Params  : 1. The heap to destroy.
 */
extern void nr_minmax_heap_destroy(nr_minmax_heap_t** heap_ptr);

/*
 * Purpose : Return the maximum size of the heap, if set.
 *
 * Params  : 1. The heap.
 *
 * Returns : The maximum size of the heap, or 0 if the heap is unbounded.
 */
extern ssize_t nr_minmax_heap_bound(const nr_minmax_heap_t* heap);

/*
 * Purpose : Return the current capacity of the heap.
 *
 * Params  : 1. The heap.
 *
 * Returns : The capacity of the heap.
 */
extern ssize_t nr_minmax_heap_capacity(const nr_minmax_heap_t* heap);

/*
 * Purpose : Return the number of elements stored in the heap.
 *
 * Params  : 1. The heap.
 *
 * Returns : The size of the heap.
 */
extern ssize_t nr_minmax_heap_size(const nr_minmax_heap_t* heap);

/*
 * Purpose : Insert an element into the heap.
 *
 * Params  : 1. The heap.
 *           2. The value to insert.
 *
 * Warning : Ownership of the value transfers to the heap; it may be immediately
 *           destroyed if the heap is bounded, full, a destructor was provided
 *           when the heap was created, and the value is smaller than the
 *           minimum value in the heap. Calling code should not assume that the
 *           value will remain valid if those conditions are met, and should
 *           clone the value before inserting it into the heap if the value is
 *           to be used subsequent to said insertion.
 */
extern void nr_minmax_heap_insert(nr_minmax_heap_t* heap, void* value);

/*
 * Purpose : Pop the minimum value from the heap and return it.
 *
 * Params  : 1. The heap.
 *
 * Returns : The value, or NULL if the heap is empty. Note that any returned
 *           value is no longer owned by the heap, and it is the responsibility
 *           of the caller to destroy it if required.
 */
extern void* nr_minmax_heap_pop_min(nr_minmax_heap_t* heap);

/*
 * Purpose : Pop the maximum value from the heap and return it.
 *
 * Params  : 1. The heap.
 *
 * Returns : The value, or NULL if the heap is empty. Note that any returned
 *           value is no longer owned by the heap, and it is the responsibility
 *           of the caller to destroy it if required.
 */
extern void* nr_minmax_heap_pop_max(nr_minmax_heap_t* heap);

/*
 * Purpose : Return the minimum value from the heap.
 *
 * Params  : 1. The heap.
 *
 * Returns : The value, or NULL if the heap is empty. Note that any returned
 *           value is still owned by the heap, and should not be destroyed or
 *           mutated.
 */
extern const void* nr_minmax_heap_peek_min(const nr_minmax_heap_t* heap);

/*
 * Purpose : Return the maximum value from the heap.
 *
 * Params  : 1. The heap.
 *
 * Returns : The value, or NULL if the heap is empty. Note that any returned
 *           value is still owned by the heap, and should not be destroyed or
 *           mutated.
 */
extern const void* nr_minmax_heap_peek_max(const nr_minmax_heap_t* heap);

/*
 * Purpose : Iterate over the values in the heap.
 *
 * Params  : 1. The heap.
 *           2. The iterator function to be invoked for each value.
 *           3. Optional userdata for the iterator.
 *
 * Note    : Ordering is not guaranteed; calling code should be prepared to
 *           receive values in any order.
 */
extern void nr_minmax_heap_iterate(const nr_minmax_heap_t* heap,
                                   nr_minmax_heap_iter_t callback,
                                   void* userdata);

#endif /* UTIL_MINMAX_HEAP_HDR */
