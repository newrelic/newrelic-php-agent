/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTIL_HASHMAP_PRIVATE_HDR
#define UTIL_HASHMAP_PRIVATE_HDR

#include "util_hashmap.h"

/*
 * A structure representing keys. Note that key values are binary safe.
 */
struct _nr_hashmap_key_t {
  size_t length;
  char* value;
};
typedef struct _nr_hashmap_key_t nr_hashmap_key_t;

/*
 * A single bucket within a hashmap. As these are implemented as a doubly
 * linked list, note that there are both prev and next pointers.
 */
struct _nr_hashmap_bucket_t {
  struct _nr_hashmap_bucket_t* prev;
  struct _nr_hashmap_bucket_t* next;
  nr_hashmap_key_t key;
  void* value;
};
typedef struct _nr_hashmap_bucket_t nr_hashmap_bucket_t;

struct _nr_hashmap_t {
  nr_hashmap_dtor_func_t dtor_func;
  size_t log2_num_buckets; /* this is the log2() of the true number of
                              buckets */
  nr_hashmap_bucket_t** buckets;
  size_t elements;
};

/*
 * Purpose : Unconditionally add a value to a hashmap.
 *
 * Notes   : 1. This function does not check parameters for validity.
 *           2. This function does not check if the key already exists; if it
 *              does a duplicate value will be added. (This will work as
 *              expected in terms of retrieval, but may cause unexpected
 *              behaviour if the element is deleted, and wastes memory.)
 */
extern void nr_hashmap_add_internal(nr_hashmap_t* hashmap,
                                    size_t hash_key,
                                    const char* key,
                                    size_t key_len,
                                    void* value);

extern nr_hashmap_t* nr_hashmap_create_internal(
    size_t log2_num_buckets,
    nr_hashmap_dtor_func_t dtor_func);

extern void nr_hashmap_destroy_bucket(nr_hashmap_bucket_t** bucket_ptr,
                                      nr_hashmap_dtor_func_t dtor_func);

extern int nr_hashmap_fetch(nr_hashmap_t* hashmap,
                            size_t hash_key,
                            const char* key,
                            size_t key_len,
                            nr_hashmap_bucket_t** bucket_ptr);

/*
 * Purpose : Calculate the hash key for the given key.
 *
 * Params  : 1. The log2_num_buckets value in the hashmap.
 *           2. The key.
 *           3. The size of the key.
 *
 * Returns : The hash key.
 */
extern size_t nr_hashmap_hash_key(size_t log2_num_buckets,
                                  const char* key,
                                  size_t key_len);

#endif /* UTIL_HASHMAP_PRIVATE_HDR */
