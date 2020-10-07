/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A basic unordered hash map, implemented using a simple hash table with
 * doubly linked lists to handle hash collisions.
 */
#ifndef UTIL_HASHMAP_HDR
#define UTIL_HASHMAP_HDR

#include <stddef.h>
#include <stdint.h>

#include "nr_axiom.h"
#include "util_memory.h"
#include "util_vector.h"

/*
 * The opaque hashmap type.
 */
typedef struct _nr_hashmap_t nr_hashmap_t;

/*
 * Type declaration for apply functions.
 */
typedef void (*nr_hashmap_apply_func_t)(void* value,
                                        const char* key,
                                        size_t key_len,
                                        void* user_data);

/*
 * Type declaration for destructor functions.
 */
typedef void (*nr_hashmap_dtor_func_t)(void* value);

/*
 * Purpose : Create a hashmap.
 *
 * Params  : 1. The destructor function for individual values, or NULL if no
 *              destructor is required.
 *
 * Returns : A newly allocated hashmap.
 */
extern nr_hashmap_t* nr_hashmap_create(nr_hashmap_dtor_func_t dtor_func);

/*
 * Purpose : Create a hashmap with a set number of buckets.
 *
 * Params  : 1. The number of buckets. If this is not a power of 2, this will
 *              be rounded up to the next power of 2. The maximum value is
 *              2^24; values above this will be capped to 2^24.
 *           2. The destructor function, or NULL if not required.
 *
 * Returns : A newly allocated hashmap.
 */
extern nr_hashmap_t* nr_hashmap_create_buckets(
    size_t buckets,
    nr_hashmap_dtor_func_t dtor_func);

/*
 * Purpose : Destroy a hashmap.
 *
 * Params  : 1. The address of the hashmap to destroy.
 */
extern void nr_hashmap_destroy(nr_hashmap_t** hashmap_ptr);

/*
 * Purpose : Apply a function to each value in a hashmap.
 *
 * Params  : 1. The hashmap.
 *           2. The function to call for each key-value pair.
 *           3. A pointer that will be passed unchanged into the apply
 *              function.
 */
extern void nr_hashmap_apply(nr_hashmap_t* hashmap,
                             nr_hashmap_apply_func_t apply_func,
                             void* user_data);

/*
 * Purpose : Count how many elements are in a hashmap.
 *
 * Params  : 1. The hashmap.
 *
 * Returns : The actual number of elements in the hashmap.
 */
extern size_t nr_hashmap_count(const nr_hashmap_t* hashmap);

/*
 * Purpose : Delete an element from a hashmap.
 *
 * Params  : 1. The hashmap.
 *           2. The key to search for.
 *           3. The length of the key.
 *
 * Returns : NR_SUCCESS if an element was found and deleted, or NR_FAILURE
 *           otherwise.
 */
extern nr_status_t nr_hashmap_delete(nr_hashmap_t* hashmap,
                                     const char* key,
                                     size_t key_len);

/*
 * Purpose : Get an element from a hashmap.
 *
 * Params  : 1. The hashmap.
 *           2. The key to search for.
 *           3. The length of the key.
 *
 * Returns : The value of the element, or NULL if it could not be found. As it
 *           is possible for the value to be NULL, you will need to use
 *           nr_hashmap_get_into or nr_hashmap_has to distinguish between these
 *           cases.
 */
extern void* nr_hashmap_get(nr_hashmap_t* hashmap,
                            const char* key,
                            size_t key_len);

/*
 * Purpose : Get an element from a hashmap into an out parameter.
 *
 * Params  : 1. The hashmap.
 *           2. The key to search for.
 *           3. The length of the key.
 *           4. A pointer to a void pointer that will be set to the element, if
 *              it exists. If the element doesn't exist, this value will be
 *              unchanged.
 *
 * Returns : Non-zero if the value exists, zero otherwise.
 */
extern int nr_hashmap_get_into(nr_hashmap_t* hashmap,
                               const char* key,
                               size_t key_len,
                               void** value_ptr);

/*
 * Purpose : Check if an element exists in a hashmap.
 *
 * Params  : 1. The hashmap.
 *           2. The key to search for.
 *           3. The length of the key.
 *
 * Returns : Non-zero if the element exists, zero otherwise.
 */
extern int nr_hashmap_has(nr_hashmap_t* hashmap,
                          const char* key,
                          size_t key_len);

/*
 * Purpose : Set an element in a hashmap. An existing element with the same key
 *           will not be overwritten by this function.
 *
 * Params  : 1. The hashmap.
 *           2. The key.
 *           3. The length of the key.
 *           4. The value.
 *
 * Returns : NR_SUCCESS if the element could be added, NR_FAILURE otherwise.
 */
extern nr_status_t nr_hashmap_set(nr_hashmap_t* hashmap,
                                  const char* key,
                                  size_t key_len,
                                  void* value);

/*
 * Purpose : Set an element in a hashmap. An existing element with the same key
 *           will be overwritten by this function.
 *
 * Params  : 1. The hashmap.
 *           2. The key.
 *           3. The length of the key.
 *           4. The value.
 */
extern void nr_hashmap_update(nr_hashmap_t* hashmap,
                              const char* key,
                              size_t key_len,
                              void* value);

/*
 * Purpose : Return a vector of all keys in the hashmap.
 *
 * Params  : 1. The hashmap.
 *
 * Returns : A vector of null-terminated strings. Freeing the vector frees all
 *           its elements.
 */
extern nr_vector_t* nr_hashmap_keys(nr_hashmap_t* hashmap);

/*
 * The below functions are simple wrappers for the main functions above that
 * allow uint64_t keys to be used.
 */
static inline nr_status_t nr_hashmap_index_delete(nr_hashmap_t* hashmap,
                                                  uint64_t index) {
  return nr_hashmap_delete(hashmap, (const char*)&index, sizeof(uint64_t));
}

static inline void* nr_hashmap_index_get(nr_hashmap_t* hashmap,
                                         uint64_t index) {
  return nr_hashmap_get(hashmap, (const char*)&index, sizeof(uint64_t));
}

static inline int nr_hashmap_index_set(nr_hashmap_t* hashmap,
                                       uint64_t index,
                                       void* value) {
  return nr_hashmap_set(hashmap, (const char*)&index, sizeof(uint64_t), value);
}

static inline void nr_hashmap_index_update(nr_hashmap_t* hashmap,
                                           uint64_t index,
                                           void* value) {
  nr_hashmap_update(hashmap, (const char*)&index, sizeof(uint64_t), value);
}

static inline void nr_hashmap_dtor_str(char* header) {
  nr_free(header);
}

#endif /* UTIL_HASHMAP_HDR */
