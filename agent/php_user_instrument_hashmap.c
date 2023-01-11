/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_user_instrument_hashmap.h"

#if ZEND_MODULE_API_NO >= ZEND_7_4_X_API_NO

typedef struct _nr_wraprecs_bucket {
  struct _nr_wraprecs_bucket* prev;
  struct _nr_wraprecs_bucket* next;
  /* for efficiency wraprec is used as both: key and a value */
  nruserfn_t* wraprec;
} nr_wraprecs_bucket_t;

typedef struct _nr_php_wraprec_hashmap {
  nr_php_wraprec_hashmap_dtor_fn_t dtor_func;
  size_t log2_num_buckets;
  nr_wraprecs_bucket_t** buckets;
  size_t elements;
} nr_php_wraprec_hashmap_t;

static inline size_t nr_count_buckets(const nr_php_wraprec_hashmap_t* hashmap) {
  return (size_t)(1 << hashmap->log2_num_buckets);
}

static nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create_internal(
    size_t log2_num_buckets,
    nr_php_wraprec_hashmap_dtor_fn_t dtor_fn) {
  nr_php_wraprec_hashmap_t* hashmap;

  if (0 == log2_num_buckets) {
    /*
     * Encode the default value in one place: namely, here.
     */
    log2_num_buckets = 8;
  } else if (log2_num_buckets > 24) {
    /*
     * Basic sanity check: it's extremely unlikely that we'll ever need a
     * hashmap for the user function wraprecs that has more than 2^24 buckets.
     */
    log2_num_buckets = 24;
  }

  hashmap
      = (nr_php_wraprec_hashmap_t*)nr_malloc(sizeof(nr_php_wraprec_hashmap_t));
  hashmap->dtor_func = dtor_fn;
  hashmap->log2_num_buckets = log2_num_buckets;
  hashmap->buckets = (nr_wraprecs_bucket_t**)nr_calloc(
      (1 << log2_num_buckets), sizeof(nr_wraprecs_bucket_t*));
  hashmap->elements = 0;

  return hashmap;
}

nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create_buckets(
    size_t buckets,
    nr_php_wraprec_hashmap_dtor_fn_t dtor_fn) {
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

  return nr_php_wraprec_hashmap_create_internal(actual_buckets, dtor_fn);
}

static void nr_destroy_wraprecs_bucket(
    nr_wraprecs_bucket_t** bucket_ptr,
    nr_php_wraprec_hashmap_dtor_fn_t dtor_func) {
  nr_wraprecs_bucket_t* bucket = *bucket_ptr;

  if (dtor_func) {
    (dtor_func)(bucket->wraprec);
  }
  nr_realfree((void**)bucket_ptr);
}

nr_php_wraprec_hashmap_stats_t nr_php_wraprec_hashmap_destroy(
    nr_php_wraprec_hashmap_t** hashmap_ptr) {
  nr_php_wraprec_hashmap_stats_t stats = {};
  size_t count;
  nr_php_wraprec_hashmap_t* hashmap;
  size_t i;
  size_t bucket_items_cnt;

  if ((NULL == hashmap_ptr) || (NULL == *hashmap_ptr)) {
    return stats;
  }
  hashmap = *hashmap_ptr;

  stats.elements = hashmap->elements;
  stats.collisions_min = stats.elements;

  count = nr_count_buckets(hashmap);
  for (i = 0; i < count; i++) {
    nr_wraprecs_bucket_t* bucket = hashmap->buckets[i];
    bucket_items_cnt = 0;

    if (bucket) {
      stats.buckets_used++;
    }

    while (bucket) {
      nr_wraprecs_bucket_t* next = bucket->next;

      bucket_items_cnt++;

      nr_destroy_wraprecs_bucket(&bucket, hashmap->dtor_func);
      bucket = next;
    }

    if (0 != bucket_items_cnt && bucket_items_cnt < stats.collisions_min) {
      stats.collisions_min = bucket_items_cnt;
    }
    if (bucket_items_cnt > stats.collisions_max) {
      stats.collisions_max = bucket_items_cnt;
    }
    if (bucket_items_cnt > 1) {
      stats.buckets_with_collisions++;
      stats.collisions_mean += bucket_items_cnt;
    }
  }

  if (0 != stats.buckets_with_collisions) {
    stats.collisions_mean /= stats.buckets_with_collisions;
  }

  nr_free(hashmap->buckets);
  nr_realfree((void**)hashmap_ptr);

  return stats;
}

static inline bool nr_zf_is_unnamed_closure(const zend_function* zf) {
  if (9 != ZSTR_LEN(zf->op_array.function_name)) {
    return false;
  }
  return 0 == memcmp("{closure}", ZSTR_VAL(zf->op_array.function_name), 9);
}

void nr_php_wraprec_hashmap_key_set(nr_php_wraprec_hashmap_key_t* key,
                                    const zend_function* zf) {
  key->lineno = nr_php_zend_function_lineno(zf);
  key->scope_name = NULL;
  key->function_name = NULL;
  key->filename = NULL;

  if (NULL != zf->op_array.function_name && !nr_zf_is_unnamed_closure(zf)) {
    key->function_name = zf->op_array.function_name;
    zend_string_addref(key->function_name);
    if (zf->op_array.scope) {
      key->scope_name = zf->op_array.scope->name;
      zend_string_addref(key->scope_name);
    }
  } else {
    if (NULL != zf->op_array.filename) {
      key->filename = zf->op_array.filename;
      zend_string_addref(key->filename);
    }
  }
}

void nr_php_wraprec_hashmap_key_release(nr_php_wraprec_hashmap_key_t* key) {
  if (NULL != key->scope_name) {
    zend_string_release(key->scope_name);
  }
  if (NULL != key->function_name) {
    zend_string_release(key->function_name);
  }
  if (NULL != key->filename) {
    zend_string_release(key->filename);
  }
  key->lineno = 0;
}

static inline size_t nr_zendfunc2bucketidx(size_t log2_num_buckets,
                                           zend_function* zf) {
  /* default to lineno */
  uint32_t hash = nr_php_zend_function_lineno(zf);

  if (NULL != zf->op_array.function_name && !nr_zf_is_unnamed_closure(zf)) {
    /* but use hash of function name when possible */
    hash = ZSTR_HASH(zf->op_array.function_name);
  } else if (NULL != zf->op_array.filename) {
    /* and as last resort try hash of filename if available */
    hash = ZSTR_HASH(zf->op_array.filename);
  }

  /* normalize to stay in-bound */
  return (hash & ((1 << log2_num_buckets) - 1));
}

static inline bool zstr_equal(zend_string* zs1, zend_string* zs2) {
  if (NULL == zs1 || NULL == zs2) {
    return false;
  }

  if (ZSTR_LEN(zs1) != ZSTR_LEN(zs2)) {
    return false;
  }

  if (ZSTR_HASH(zs1) != ZSTR_HASH(zs2)) {
    return false;
  }

  if (0 != memcmp(ZSTR_VAL(zs1), ZSTR_VAL(zs2), ZSTR_LEN(zs1))) {
    return false;
  }

  return true;
}

static bool nr_is_wraprec_for_zend_func(nr_php_wraprec_hashmap_key_t* key,
                                        zend_function* zf) {
  /* Start with comparing line number: */
  if (nr_php_zend_function_lineno(zf) != key->lineno) {
    /* no match: line number is different - no need to check anything else */
    return false;
  }

  /* Next compare function name unless it is unnamed closure. Zend engine sets
   * function name to '{closure}' for all unnamed closures so function name
   * cannot be used for them. A fallback method to compare filename is used
   * for unnamed closures. */
  if (zf->op_array.function_name && !nr_zf_is_unnamed_closure(zf)) {
    if (!zstr_equal(key->function_name, zf->op_array.function_name)) {
      /* no match: function name is different - no need to check anything else
       */
      return false;
    }
    /* If function is scoped, compare the scope: */
    if (zf->op_array.scope
        && !zstr_equal(key->scope_name, zf->op_array.scope->name)) {
      /* no match: scope is different */
      return false;
    }
    /* match: line number, function name and scope (if function is scoped) are
     * the same */
    return true;
  }

  /* deal with unnamed colsure: fallback to comparing filename */
  if (!zstr_equal(key->filename, zf->op_array.filename)) {
    /* no match: filename is different */
    return false;
  }

  /* match: line number and filename are the same */
  return true;
}

static int nr_php_wraprec_hashmap_fetch_internal(
    nr_php_wraprec_hashmap_t* hashmap,
    size_t hash_key,
    zend_function* zf,
    nr_wraprecs_bucket_t** bucket_ptr) {
  nr_wraprecs_bucket_t* bucket;

  for (bucket = hashmap->buckets[hash_key]; bucket; bucket = bucket->next) {
    if (nr_is_wraprec_for_zend_func(&bucket->wraprec->key, zf)) {
      *bucket_ptr = bucket;
      return 1;
    }
  }

  return 0;
}

static void nr_php_wraprec_hashmap_add_internal(
    nr_php_wraprec_hashmap_t* hashmap,
    size_t hash_key,
    nruserfn_t* wr) {
  nr_wraprecs_bucket_t* bucket;

  bucket = (nr_wraprecs_bucket_t*)nr_malloc(sizeof(nr_wraprecs_bucket_t));
  bucket->prev = NULL;
  bucket->next = hashmap->buckets[hash_key];
  bucket->wraprec = wr;

  if (hashmap->buckets[hash_key]) {
    hashmap->buckets[hash_key]->prev = bucket;
  }

  hashmap->buckets[hash_key] = bucket;
  ++hashmap->elements;
}

void nr_php_wraprec_hashmap_update(nr_php_wraprec_hashmap_t* hashmap,
                                   zend_function* zf,
                                   nruserfn_t* wr) {
  nr_wraprecs_bucket_t* bucket = NULL;
  size_t bucketidx;

  if ((NULL == hashmap) || (NULL == zf) || (0 == wr)) {
    return;
  }

  nr_php_wraprec_hashmap_key_set(&wr->key, zf);

  bucketidx = nr_zendfunc2bucketidx(hashmap->log2_num_buckets, zf);
  if (nr_php_wraprec_hashmap_fetch_internal(hashmap, bucketidx, zf, &bucket)) {
    if (hashmap->dtor_func) {
      (hashmap->dtor_func)(bucket->wraprec);
    }

    bucket->wraprec = wr;
    return;
  }

  nr_php_wraprec_hashmap_add_internal(hashmap, bucketidx, wr);
}

int nr_php_wraprec_hashmap_get_into(nr_php_wraprec_hashmap_t* hashmap,
                                    zend_function* zf,
                                    nruserfn_t** wraprec_ptr) {
  nr_wraprecs_bucket_t* bucket = NULL;
  size_t bucketidx;

  if ((NULL == hashmap) || (NULL == zf) || (NULL == wraprec_ptr)) {
    return 0;
  }

  bucketidx = nr_zendfunc2bucketidx(hashmap->log2_num_buckets, zf);
  if (nr_php_wraprec_hashmap_fetch_internal(hashmap, bucketidx, zf, &bucket)) {
    *wraprec_ptr = bucket->wraprec;
    return 1;
  }

  return 0;
}

#endif