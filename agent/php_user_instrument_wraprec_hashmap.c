/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_user_instrument_wraprec_hashmap.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

// -----------------------------------------------------------------------------
// func hash map

typedef struct {
  bool is_method;
  const char* name;
  int name_len;
  zend_ulong name_hash;
} nr_func_hashmap_key_t;

typedef struct _nr_func_bucket {
  struct _nr_func_bucket* prev;
  struct _nr_func_bucket* next;
  nr_func_hashmap_key_t* key;
  nruserfn_t* wraprec;
} nr_func_bucket_t;

typedef struct _nr_func_hashmap {
  size_t log2_num_buckets;
  nr_func_bucket_t** buckets;
  size_t elements;
} nr_func_hashmap_t;

static nr_func_hashmap_t* nr_func_hashmap_create_internal(size_t log2_num_buckets) {
  nr_func_hashmap_t* hashmap;

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
      = (nr_func_hashmap_t*)nr_malloc(sizeof(nr_func_hashmap_t));
  hashmap->log2_num_buckets = log2_num_buckets;
  hashmap->buckets = (nr_func_bucket_t**)nr_calloc(
      (1 << log2_num_buckets), sizeof(nr_func_bucket_t*));
  hashmap->elements = 0;

  return hashmap;
}

static size_t nr_func_hashmap_hash_key(size_t log2_num_buckets, nr_func_hashmap_key_t* key) {
  return (key->name_hash & ((1 << log2_num_buckets) - 1));
}

static bool nr_func_hashmap_key_equals(nr_func_hashmap_key_t* a, nr_func_hashmap_key_t* b) {
  return (a->name_hash == b->name_hash) && (a->name_len == b->name_len)
         && (0 == nr_strncmp(a->name, b->name, a->name_len));
}

static bool nr_func_hashmap_fetch_internal(nr_func_hashmap_t* hashmap, size_t hash, nr_func_hashmap_key_t* key, nr_func_bucket_t** bucket_ptr) {
  nr_func_bucket_t* bucket;
  for (bucket = hashmap->buckets[hash]; bucket; bucket = bucket->next) {
    if (nr_func_hashmap_key_equals(bucket->key, key)) {
      if (bucket_ptr) {
        *bucket_ptr = bucket;
      }
      return true;
    }
  }
  return false;
}

static nruserfn_t* nr_func_hashmap_add_internal(nr_func_hashmap_t* hashmap,
                             size_t hash_key,
                             nr_func_hashmap_key_t* key) {
  nr_func_bucket_t* bucket;

  bucket = (nr_func_bucket_t*)nr_malloc(sizeof(nr_func_bucket_t));
  bucket->prev = NULL;
  bucket->next = hashmap->buckets[hash_key];
  bucket->key = (nr_func_hashmap_key_t*)nr_malloc(sizeof(nr_func_hashmap_key_t));
  bucket->key->is_method = key->is_method;
  bucket->key->name = nr_strndup(key->name, key->name_len);
  bucket->key->name_len = key->name_len;
  bucket->key->name_hash = key->name_hash;
  bucket->wraprec = nr_php_user_wraprec_create();

  if (hashmap->buckets[hash_key]) {
    hashmap->buckets[hash_key]->prev = bucket;
  }

  hashmap->buckets[hash_key] = bucket;
  ++hashmap->elements;

  return bucket->wraprec;
}

static nruserfn_t* nr_func_hashmap_lookup_internal(nr_func_hashmap_t* hashmap, nr_func_hashmap_key_t* key) {
  size_t hash;
  nr_func_bucket_t* bucket;

  if (nrunlikely((NULL == hashmap) || (NULL == key))) {
    return NULL;
  }

  hash = nr_func_hashmap_hash_key(hashmap->log2_num_buckets, key);
  if (nr_func_hashmap_fetch_internal(hashmap, hash, key, &bucket)) {
    return bucket->wraprec;
  }

  return NULL;
}

static nruserfn_t* nr_func_hashmap_update_internal(nr_func_hashmap_t* hashmap, nr_func_hashmap_key_t* key, bool* created) {
  size_t hash;
  nr_func_bucket_t* bucket;
  nruserfn_t* wraprec;

  if (nrunlikely((NULL == hashmap) || (NULL == key))) {
    return NULL;
  }

  hash = nr_func_hashmap_hash_key(hashmap->log2_num_buckets, key);
  if (nr_func_hashmap_fetch_internal(hashmap, hash, key, &bucket)) {
    if (created) {
      *created = false;
    }
    return bucket->wraprec;
  }

  if (created) {
    *created = true;
  }
  return nr_func_hashmap_add_internal(hashmap, hash, key);
}

static void nr_func_hashmap_destroy_bucket_internal(nr_func_bucket_t** bucket_ptr) {
  nr_func_bucket_t* bucket = *bucket_ptr;
  nr_free(bucket->key->name);
  nr_free(bucket->key);
  nr_php_user_wraprec_destroy(&bucket->wraprec);
  nr_realfree((void**)bucket_ptr);
}

static void nr_func_hashmap_destroy_internal(nr_func_hashmap_t** hashmap_ptr) {
  size_t count;
  nr_func_hashmap_t* hashmap;
  size_t i;

  if ((NULL == hashmap_ptr) || (NULL == *hashmap_ptr)) {
    return;
  }
  hashmap = *hashmap_ptr;

  count = (size_t)(1 << hashmap->log2_num_buckets);
  for (i = 0; i < count; i++) {
    nr_func_bucket_t* bucket = hashmap->buckets[i];

    while (bucket) {
      nr_func_bucket_t* next = bucket->next;

      nr_func_hashmap_destroy_bucket_internal(&bucket);

      bucket = next;
    }
  }

  nr_free(hashmap->buckets);
  nr_realfree((void**)hashmap_ptr);
}

// -----------------------------------------------------------------------------
// scope hash map

typedef struct {
  const char* name;
  int name_len;
  zend_ulong name_hash;
} nr_scope_hashmap_key_t;

typedef struct _nr_scope_bucket {
  struct _nr_scope_bucket* prev;
  struct _nr_scope_bucket* next;
  nr_scope_hashmap_key_t* key;
  nr_func_hashmap_t* scoped_funcs_ht;
} nr_scope_bucket_t;

typedef struct _nr_scope_hashmap {
  size_t log2_num_buckets;
  nr_scope_bucket_t** buckets;
  size_t elements;
} nr_scope_hashmap_t;

static nr_scope_hashmap_t* nr_scope_hashmap_create_internal(size_t log2_num_buckets) {
  nr_scope_hashmap_t* hashmap;

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
      = (nr_scope_hashmap_t*)nr_malloc(sizeof(nr_scope_hashmap_t));
  hashmap->log2_num_buckets = log2_num_buckets;
  hashmap->buckets = (nr_scope_bucket_t**)nr_calloc(
      (1 << log2_num_buckets), sizeof(nr_scope_bucket_t*));
  hashmap->elements = 0;

  return hashmap;
}

static size_t nr_scope_hashmap_hash_key(size_t log2_num_buckets, nr_scope_hashmap_key_t* key) {
  return (key->name_hash & ((1 << log2_num_buckets) - 1));
}

static bool nr_scope_hashmap_key_equals(nr_scope_hashmap_key_t* a, nr_scope_hashmap_key_t* b) {
  return (a->name_hash == b->name_hash) && (a->name_len == b->name_len)
         && (0 == nr_strncmp(a->name, b->name, a->name_len));
}

static bool nr_scope_hashmap_fetch_internal(nr_scope_hashmap_t* hashmap, size_t hash, nr_scope_hashmap_key_t* key, nr_scope_bucket_t** bucket_ptr) {
  nr_scope_bucket_t* bucket;
  for (bucket = hashmap->buckets[hash]; bucket; bucket = bucket->next) {
    if (nr_scope_hashmap_key_equals(bucket->key, key)) {
      if (bucket_ptr) {
        *bucket_ptr = bucket;
      }
      return true;
    }
  }
  return false;
}

static nr_func_hashmap_t* nr_scope_hashmap_add_internal(nr_scope_hashmap_t* hashmap,
                             size_t hash_key,
                             nr_scope_hashmap_key_t* key) {
  nr_scope_bucket_t* bucket;

  bucket = (nr_scope_bucket_t*)nr_malloc(sizeof(nr_scope_bucket_t));
  bucket->prev = NULL;
  bucket->next = hashmap->buckets[hash_key];
  bucket->key = (nr_scope_hashmap_key_t*)nr_malloc(sizeof(nr_scope_hashmap_key_t));
  bucket->key->name = nr_strndup(key->name, key->name_len);
  bucket->key->name_len = key->name_len;
  bucket->key->name_hash = key->name_hash;
  bucket->scoped_funcs_ht = nr_func_hashmap_create_internal(0);

  if (hashmap->buckets[hash_key]) {
    hashmap->buckets[hash_key]->prev = bucket;
  }

  hashmap->buckets[hash_key] = bucket;
  ++hashmap->elements;

  return bucket->scoped_funcs_ht;
}

static nr_func_hashmap_t* nr_scope_hashmap_lookup_internal(nr_scope_hashmap_t* hashmap, nr_scope_hashmap_key_t* key) {
  size_t hash;
  nr_scope_bucket_t* bucket;

  if (nrunlikely((NULL == hashmap) || (NULL == key))) {
    return NULL;
  }

  hash = nr_scope_hashmap_hash_key(hashmap->log2_num_buckets, key);
  if (nr_scope_hashmap_fetch_internal(hashmap, hash, key, &bucket)) {
    return bucket->scoped_funcs_ht;
  }

  return NULL;
}

static nr_func_hashmap_t* nr_scope_hashmap_update_internal(nr_scope_hashmap_t* hashmap, nr_scope_hashmap_key_t* key) {
  size_t hash;
  nr_scope_bucket_t* bucket;

  if (nrunlikely((NULL == hashmap) || (NULL == key))) {
    return NULL;
  }

  hash = nr_scope_hashmap_hash_key(hashmap->log2_num_buckets, key);
  if (nr_scope_hashmap_fetch_internal(hashmap, hash, key, &bucket)) {
    return bucket->scoped_funcs_ht;
  }

  return nr_scope_hashmap_add_internal(hashmap, hash, key);
}

static void nr_scope_hashmap_destroy_bucket_internal(nr_scope_bucket_t** bucket_ptr) {
  nr_scope_bucket_t* bucket = *bucket_ptr;
  nr_free(bucket->key->name);
  nr_free(bucket->key);
  nr_func_hashmap_destroy_internal(&bucket->scoped_funcs_ht);
  nr_realfree((void**)bucket_ptr);
}

static void nr_scope_hashmap_destroy_internal(nr_scope_hashmap_t** hashmap_ptr) {
  size_t count;
  nr_scope_hashmap_t* hashmap;
  size_t i;

  if ((NULL == hashmap_ptr) || (NULL == *hashmap_ptr)) {
    return;
  }
  hashmap = *hashmap_ptr;

  count = (size_t)(1 << hashmap->log2_num_buckets);
  for (i = 0; i < count; i++) {
    nr_scope_bucket_t* bucket = hashmap->buckets[i];

    while (bucket) {
      nr_scope_bucket_t* next = bucket->next;

      nr_scope_hashmap_destroy_bucket_internal(&bucket);

      bucket = next;
    }
  }

  nr_free(hashmap->buckets);
  nr_realfree((void**)hashmap_ptr);
}

nr_func_hashmap_t* global_funcs_ht = NULL;
nr_scope_hashmap_t* scope_ht = NULL;

static void nr_php_user_instrument_wraprec_hashmap_name2keys(
  nr_func_hashmap_key_t* func,
  nr_scope_hashmap_key_t* scope,
  const char* full_name,
  int full_name_len) {

  if (nrunlikely(NULL == full_name)) {
    return;
  }
  if (nrunlikely(full_name_len <= 0)) {
    return;
  }

  func->name = full_name;
  func->name_len = full_name_len;
  func->name_hash = 0;
  scope->name = NULL;
  scope->name_len = 0;
  scope->name_hash = 0;

  /* If scope::method, then break into two strings */
  for (int i = 0; i < full_name_len; i++) {
    if ((':' == full_name[i]) && (':' == full_name[i + 1])) {
      func->is_method = true;
      scope->name = full_name;
      scope->name_len = i;
      func->name = full_name + i + 2;
      func->name_len = full_name_len - i - 2;
    }
  }

  if (func->is_method) {
    scope->name_hash = zend_hash_func(scope->name, scope->name_len);
  }
  func->name_hash = zend_hash_func(func->name, func->name_len);
}

nr_status_t nr_php_user_instrument_wraprec_hashmap_init(void) {
  if (NULL == scope_ht) {
    scope_ht = nr_scope_hashmap_create_internal(0);
  }
  if (NULL == global_funcs_ht) {
    global_funcs_ht = nr_func_hashmap_create_internal(0);
  }
  return NR_SUCCESS;
}

nruserfn_t* nr_php_user_instrument_wraprec_hashmap_add(const char* namestr, size_t namestrlen) {
  nr_scope_hashmap_key_t scope_key = {0};
  nr_func_hashmap_key_t func_key = {0};
  nr_func_hashmap_t* funcs_ht = NULL;
  bool is_new_wraprec = false;
  nruserfn_t* wraprec = NULL;


  if (NULL == scope_ht || NULL == global_funcs_ht) {
    return NULL;
  }
  if (NULL == namestr || 0 == namestrlen) {
    return NULL;
  }

  nr_php_user_instrument_wraprec_hashmap_name2keys(&func_key, &scope_key, namestr, namestrlen);

  if (func_key.is_method) {
    funcs_ht = nr_scope_hashmap_update_internal(scope_ht, &scope_key);
  } else {
    funcs_ht = global_funcs_ht;
  }

  if (NULL == funcs_ht) {
    return NULL;
  }

  wraprec = nr_func_hashmap_update_internal(funcs_ht, &func_key, &is_new_wraprec);

  if (NULL == wraprec) {
    return NULL;
  }

  if (is_new_wraprec) {
    wraprec->funcname = nr_strndup(func_key.name, func_key.name_len);
    wraprec->funcnamelen = func_key.name_len;
    wraprec->funcnameLC = nr_string_to_lowercase(wraprec->funcname);
    if (func_key.is_method) {
      wraprec->classname = nr_strndup(scope_key.name, scope_key.name_len);
      wraprec->classnamelen = scope_key.name_len;
      wraprec->classnameLC = nr_string_to_lowercase(wraprec->classname);
      wraprec->is_method = 1;
    }

    wraprec->supportability_metric = nr_txn_create_fn_supportability_metric(
        wraprec->funcname, wraprec->classname);
  }

  return wraprec;
}

nruserfn_t* nr_php_user_instrument_wraprec_hashmap_get(zend_string *func_name, zend_string *scope_name) {
  nr_scope_hashmap_key_t scope_key = {0};
  nr_func_hashmap_key_t func_key = {0};
  nr_func_hashmap_t* funcs_ht = NULL;

  if (NULL == scope_ht || NULL == global_funcs_ht) {
    return NULL;
  }
  if (NULL == func_name) {
    return NULL;
  }

  if (NULL != scope_name) {
    func_key.is_method = true;
    scope_key.name = ZSTR_VAL(scope_name);
    scope_key.name_len = ZSTR_LEN(scope_name);
    scope_key.name_hash = ZSTR_HASH(scope_name);
    funcs_ht = nr_scope_hashmap_lookup_internal(scope_ht, &scope_key);
  } else {
    funcs_ht = global_funcs_ht;
  }

  if (NULL == funcs_ht) {
    return NULL;
  }

  func_key.name = ZSTR_VAL(func_name);
  func_key.name_len = ZSTR_LEN(func_name);
  func_key.name_hash = ZSTR_HASH(func_name);

  return nr_func_hashmap_lookup_internal(funcs_ht, &func_key);
}

void nr_php_user_instrument_wraprec_hashmap_destroy(void) {
  if (NULL != scope_ht) {
    nr_scope_hashmap_destroy_internal(&scope_ht);
  }
  if (NULL != global_funcs_ht) {
    nr_func_hashmap_destroy_internal(&global_funcs_ht);
  }
  return;
}
