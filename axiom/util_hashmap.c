/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_hash.h"
#include "util_hashmap.h"
#include "util_hashmap_private.h"
#include "util_memory.h"

static inline size_t nr_hashmap_count_buckets(const nr_hashmap_t* hashmap) {
  return (size_t)(1 << hashmap->log2_num_buckets);
}

nr_hashmap_t* nr_hashmap_create(nr_hashmap_dtor_func_t dtor_func) {
  return nr_hashmap_create_internal(0, dtor_func);
}

nr_hashmap_t* nr_hashmap_create_buckets(size_t buckets,
                                        nr_hashmap_dtor_func_t dtor_func) {
  size_t actual_buckets;

  if (0 == buckets) {
    actual_buckets = 0;
  } else {
    /*
     * Calculate ceil(log2(buckets)), since that's what
     * nr_hashmap_create_internal requires.
     */

    actual_buckets = 1;
    while (((size_t)(1 << actual_buckets)) < buckets) {
      actual_buckets += 1;
    }
  }

  return nr_hashmap_create_internal(actual_buckets, dtor_func);
}

void nr_hashmap_destroy(nr_hashmap_t** hashmap_ptr) {
  size_t count;
  nr_hashmap_t* hashmap;
  size_t i;

  if ((NULL == hashmap_ptr) || (NULL == *hashmap_ptr)) {
    return;
  }
  hashmap = *hashmap_ptr;

  count = nr_hashmap_count_buckets(hashmap);
  for (i = 0; i < count; i++) {
    nr_hashmap_bucket_t* bucket = hashmap->buckets[i];

    while (bucket) {
      nr_hashmap_bucket_t* next = bucket->next;

      nr_hashmap_destroy_bucket(&bucket, hashmap->dtor_func);
      bucket = next;
    }
  }

  nr_free(hashmap->buckets);
  nr_realfree((void**)hashmap_ptr);
}

void nr_hashmap_apply(nr_hashmap_t* hashmap,
                      nr_hashmap_apply_func_t apply_func,
                      void* user_data) {
  size_t count;
  size_t i;

  if ((NULL == hashmap) || (NULL == apply_func)) {
    return;
  }

  count = nr_hashmap_count_buckets(hashmap);
  for (i = 0; i < count; i++) {
    nr_hashmap_bucket_t* bucket;

    for (bucket = hashmap->buckets[i]; bucket; bucket = bucket->next) {
      (apply_func)(bucket->value, bucket->key.value, bucket->key.length,
                   user_data);
    }
  }
}

size_t nr_hashmap_count(const nr_hashmap_t* hashmap) {
  if (NULL == hashmap) {
    return 0;
  }

  return hashmap->elements;
}

nr_status_t nr_hashmap_delete(nr_hashmap_t* hashmap,
                              const char* key,
                              size_t key_len) {
  nr_hashmap_bucket_t* bucket = NULL;
  size_t hash;

  if ((NULL == hashmap) || (NULL == key) || (0 == key_len)) {
    return NR_FAILURE;
  }

  hash = nr_hashmap_hash_key(hashmap->log2_num_buckets, key, key_len);
  if (nr_hashmap_fetch(hashmap, hash, key, key_len, &bucket)) {
    /*
     * Patch up the doubly linked list so that we don't point at something
     * we're about to free.
     */
    if (bucket->next) {
      bucket->next->prev = bucket->prev;
    }

    if (bucket->prev) {
      bucket->prev->next = bucket->next;
    }

    /*
     * If this is the first bucket in the list for this hash key, we need to
     * point the head of the list at the next bucket.
     */
    if (hashmap->buckets[hash] == bucket) {
      hashmap->buckets[hash] = bucket->next;
    }

    nr_hashmap_destroy_bucket(&bucket, hashmap->dtor_func);
    hashmap->elements -= 1;

    return NR_SUCCESS;
  }

  return NR_FAILURE;
}

void* nr_hashmap_get(nr_hashmap_t* hashmap, const char* key, size_t key_len) {
  void* value = NULL;

  if (nr_hashmap_get_into(hashmap, key, key_len, &value)) {
    return value;
  }

  return NULL;
}

int nr_hashmap_get_into(nr_hashmap_t* hashmap,
                        const char* key,
                        size_t key_len,
                        void** value_ptr) {
  nr_hashmap_bucket_t* bucket = NULL;
  size_t hash;

  if ((NULL == hashmap) || (NULL == key) || (0 == key_len)
      || (NULL == value_ptr)) {
    return 0;
  }

  hash = nr_hashmap_hash_key(hashmap->log2_num_buckets, key, key_len);
  if (nr_hashmap_fetch(hashmap, hash, key, key_len, &bucket)) {
    *value_ptr = bucket->value;
    return 1;
  }

  return 0;
}

int nr_hashmap_has(nr_hashmap_t* hashmap, const char* key, size_t key_len) {
  size_t hash;

  if ((NULL == hashmap) || (NULL == key) || (0 == key_len)) {
    return 0;
  }

  hash = nr_hashmap_hash_key(hashmap->log2_num_buckets, key, key_len);
  return nr_hashmap_fetch(hashmap, hash, key, key_len, NULL);
}

nr_status_t nr_hashmap_set(nr_hashmap_t* hashmap,
                           const char* key,
                           size_t key_len,
                           void* value) {
  size_t hash;

  if ((NULL == hashmap) || (NULL == key) || (0 == key_len)) {
    return NR_FAILURE;
  }

  hash = nr_hashmap_hash_key(hashmap->log2_num_buckets, key, key_len);
  if (nr_hashmap_fetch(hashmap, hash, key, key_len, NULL)) {
    return NR_FAILURE;
  }

  nr_hashmap_add_internal(hashmap, hash, key, key_len, value);
  return NR_SUCCESS;
}

void nr_hashmap_update(nr_hashmap_t* hashmap,
                       const char* key,
                       size_t key_len,
                       void* value) {
  nr_hashmap_bucket_t* bucket = NULL;
  size_t hash;

  if ((NULL == hashmap) || (NULL == key) || (0 == key_len)) {
    return;
  }

  hash = nr_hashmap_hash_key(hashmap->log2_num_buckets, key, key_len);
  if (nr_hashmap_fetch(hashmap, hash, key, key_len, &bucket)) {
    if (hashmap->dtor_func) {
      (hashmap->dtor_func)(bucket->value);
    }

    bucket->value = value;
    return;
  }

  nr_hashmap_add_internal(hashmap, hash, key, key_len, value);
}

void nr_hashmap_add_internal(nr_hashmap_t* hashmap,
                             size_t hash_key,
                             const char* key,
                             size_t key_len,
                             void* value) {
  nr_hashmap_bucket_t* bucket;

  bucket = (nr_hashmap_bucket_t*)nr_malloc(sizeof(nr_hashmap_bucket_t));
  bucket->prev = NULL;
  bucket->next = hashmap->buckets[hash_key];
  bucket->key.length = key_len;
  bucket->key.value = (char*)nr_malloc(key_len);
  nr_memcpy(bucket->key.value, key, key_len);
  bucket->value = value;

  if (hashmap->buckets[hash_key]) {
    hashmap->buckets[hash_key]->prev = bucket;
  }

  hashmap->buckets[hash_key] = bucket;
  ++hashmap->elements;
}

nr_hashmap_t* nr_hashmap_create_internal(size_t log2_num_buckets,
                                         nr_hashmap_dtor_func_t dtor_func) {
  nr_hashmap_t* hashmap;

  if (0 == log2_num_buckets) {
    /*
     * Encode the default value in one place: namely, here.
     */
    log2_num_buckets = 8;
  } else if (log2_num_buckets > 24) {
    /*
     * Basic sanity check: it's extremely unlikely that we'll ever need a
     * hashmap for the agent that has more than 2^24 buckets.
     */
    log2_num_buckets = 24;
  }

  hashmap = (nr_hashmap_t*)nr_malloc(sizeof(nr_hashmap_t));
  hashmap->dtor_func = dtor_func;
  hashmap->log2_num_buckets = log2_num_buckets;
  hashmap->buckets = (nr_hashmap_bucket_t**)nr_calloc(
      (1 << log2_num_buckets), sizeof(nr_hashmap_bucket_t*));
  hashmap->elements = 0;

  return hashmap;
}

void nr_hashmap_destroy_bucket(nr_hashmap_bucket_t** bucket_ptr,
                               nr_hashmap_dtor_func_t dtor_func) {
  nr_hashmap_bucket_t* bucket = *bucket_ptr;

  nr_free(bucket->key.value);
  if (dtor_func) {
    (dtor_func)(bucket->value);
  }
  nr_realfree((void**)bucket_ptr);
}

int nr_hashmap_fetch(nr_hashmap_t* hashmap,
                     size_t hash_key,
                     const char* key,
                     size_t key_len,
                     nr_hashmap_bucket_t** bucket_ptr) {
  nr_hashmap_bucket_t* bucket;

  for (bucket = hashmap->buckets[hash_key]; bucket; bucket = bucket->next) {
    if (key_len == bucket->key.length) {
      if (0 == nr_memcmp(key, bucket->key.value, key_len)) {
        if (bucket_ptr) {
          *bucket_ptr = bucket;
        }
        return 1;
      }
    }
  }

  return 0;
}

size_t nr_hashmap_hash_key(size_t log2_num_buckets,
                           const char* key,
                           size_t key_len) {
  int len = (int)key_len;
  uint32_t hash = nr_mkhash(key, &len);

  /*
   * There's an implicit assumption here that nr_mkhash will return a
   * reasonably well distributed hash value, and therefore that using the low
   * bits for truncation is OK. If MurmurHash3 (the hash function we're using
   * at the time of writing this) turns out to be less well distributed than we
   * may hope, then we should evaluate whether another function can be used,
   * and whether we should implement and use a function just for the hashmap
   * structure with the qualities we want.
   */

  return (hash & ((1 << log2_num_buckets) - 1));
}

static void nr_hashmap_keys_destroy_key(void* key, void* userdata NRUNUSED) {
  if (NULL == key) {
    return;
  }

  nr_free(key);
}

static void nr_hashmap_keys_add_key(void* value NRUNUSED,
                                    const char* key,
                                    size_t keylen,
                                    void* userdata) {
  nr_vector_t* keys = (nr_vector_t*)userdata;
  char* key_copy;

  if (NULL == keys || NULL == userdata) {
    return;
  }

  key_copy = nr_strndup(key, keylen);

  if (NULL == key_copy) {
    return;
  }

  nr_vector_push_back(keys, key_copy);
}

nr_vector_t* nr_hashmap_keys(nr_hashmap_t* hashmap) {
  nr_vector_t* keys;

  if (NULL == hashmap) {
    return NULL;
  }

  keys = nr_vector_create(nr_hashmap_count(hashmap),
                          nr_hashmap_keys_destroy_key, NULL);

  nr_hashmap_apply(hashmap, nr_hashmap_keys_add_key, keys);

  return keys;
}
