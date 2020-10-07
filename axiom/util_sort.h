/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_SORT_HDR
#define UTIL_SORT_HDR

#include <stddef.h>

typedef int (*nr_sort_cmp_t)(const void* a, const void* b, void* userdata);

/*
 * Purpose : Sort an array.
 *
 *           This is effectively a wrapper for qsort_r() on systems that support
 *           it, just with a consistent argument order in the callback function.
 *
 *           On systems that do not have a qsort_r() implementation, this
 *           function provides the same behaviour in a thread safe manner.
 *
 * Params  : 1. The array to sort.
 *           2. The number of elements within the array.
 *           3. The size of each element within the array.
 *           4. The comparison function to use. It must return an integer less
 *              than, equal to, or greater than zero if the first argument is
 *              considered less than, equal to, or greater than the second
 *              argument, respectively.
 *           5. An opaque user data pointer which will be passed to the
 *              callback.
 */
extern void nr_sort(void* base,
                    size_t nmemb,
                    size_t size,
                    nr_sort_cmp_t compar,
                    void* arg);

#endif /* UTIL_SORT_HDR */
