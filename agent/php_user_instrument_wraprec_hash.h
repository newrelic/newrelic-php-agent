/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#if LOOKUP_METHOD == LOOKUP_USE_WRAPREC_HASHMAP

/* This hashmap will store pointers to wraprecs stored in the linked list.
 * Since wraprecs are persistent (they're not destroyed between requests), 
 * there's no need for hashmap value destructor. */

typedef struct _nr_wraprec_hashmap_bucket_t {
  struct _nr_wraprec_hashmap_bucket_t* prev;
  struct _nr_wraprec_hashmap_bucket_t* next;
  nruserfn_t *wraprec;
} nr_wraprec_hashmap_bucket_t;

struct _nr_wraprec_hashmap_t {
  size_t hash_modulo;
  nr_wraprec_hashmap_bucket_t** buckets;
  size_t elements;
};

#ifdef DECLARE_nr_php_wraprec_hashmap_API
extern nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create(void);
#else
nr_php_wraprec_hashmap_t* nr_php_wraprec_hashmap_create() {
  nr_php_wraprec_hashmap_t* hashmap;

  hashmap = (nr_php_wraprec_hashmap_t*)nr_malloc(sizeof(nr_php_wraprec_hashmap_t));
    /*
     * Encode the default value in one place: namely, here.
     */
  hashmap->hash_modulo = 257; //521 or 509 //53; // or 97 or other prime number
  hashmap->buckets = (nr_wraprec_hashmap_bucket_t**)nr_calloc(
      hashmap->hash_modulo, sizeof(nr_wraprec_hashmap_bucket_t*));
  hashmap->elements = 0;

  return hashmap;
}
#endif

#ifdef DECLARE_nr_php_wraprec_hashmap_API
extern void nr_php_wraprec_hashmap_destroy(nr_php_wraprec_hashmap_t**);
#else
void nr_php_wraprec_hashmap_destroy(nr_php_wraprec_hashmap_t** hashmap_ptr) {
  nr_php_wraprec_hashmap_t* hashmap;
  size_t i;

  if ((NULL == hashmap_ptr) || (NULL == *hashmap_ptr)) {
    return;
  }
  hashmap = *hashmap_ptr;

  for (i = 0; i < hashmap->hash_modulo; i++) {
    nr_wraprec_hashmap_bucket_t* bucket = hashmap->buckets[i];

    while (bucket) {
      nr_wraprec_hashmap_bucket_t* next = bucket->next;

      nr_realfree((void **)&bucket);
      bucket = next;
    }
  }

  nr_free(hashmap->buckets);
  nr_realfree((void**)hashmap_ptr);
}
#endif

#define SET_META_COMMON(id, zf) do { \
  if (nrunlikely(NULL == id || NULL == zf)) { \
    return; \
  } \
  id->lineno = nr_php_zend_function_lineno(zf); \
} while(0)

#define SET_META_STRING(id, nr_metaname, zf, zf_metaname, method) do { \
  const char *metavalue = nr_php_op_array_ ## zf_metaname(&zf->op_array); \
  const size_t metavaluelen = nr_php_op_array_ ## zf_metaname ## _length(&zf->op_array); \
  if (NULL != metavalue) { \
    id->nr_metaname.is_set = true; \
    id->nr_metaname.len = metavaluelen; \
    if (NULL == method) { \
      id->nr_metaname.value.ccp = metavalue; \
    } else { \
      id->nr_metaname.value.cp = nr_strdup(metavalue); \
    } \
  } else { \
    id->nr_metaname.is_set = true; \
  } \
} while (0)

/* Cast zend_function to nruserfn_metadata - id will point to metadata in zf.
 * Cast does not make any memory allocation but makes the code more readable */
static inline void zf_as_id(nruserfn_metadata* id, const zend_function* zf) {
  SET_META_COMMON(id, zf);
  SET_META_STRING(id, filename, zf, file_name, NULL);
  SET_META_STRING(id, scope, zf, scope_name, NULL);
  SET_META_STRING(id, function_name, zf, function_name, NULL);
}

/* Copy zend_function metadata to nruserfn_metadata - id will have copy zf's
 * metadata. Copy allocates memory that the caller must free. */
static inline void zf_to_id(nruserfn_metadata* id, const zend_function* zf) {
  SET_META_COMMON(id, zf);
  SET_META_STRING(id, filename, zf, file_name, nr_strdup);
  SET_META_STRING(id, scope, zf, scope_name, nr_strdup);
  SET_META_STRING(id, function_name, zf, function_name, nr_strdup);
}

static inline void wraprec_metadata_set(nruserfn_metadata* id, const zend_function* zf) {
  zf_to_id(id, zf);
}

static inline bool wraprec_streq(const zf_metadata_t *wr_id, const zf_metadata_t* zf_id) {

  if (!wr_id->is_set || !zf_id->is_set) {
    return false;
  }

  /* This is an assert - it's already been checked */
  if (wr_id->len != zf_id->len) {
    return false;
  }

  /* compare from the back */
  for (int i = wr_id->len; i >= 0; i--) {
    if (wr_id->value.ccp[i] != zf_id->value.ccp[i]) {
      return false;
    }
  }

  return true;
}

static inline bool wraprec_metadata_is_match(const nruserfn_t* wraprec, const zend_function* zf) {
  const nruserfn_metadata *nr_id;
  nruserfn_metadata zf_id = {};

  if (nrunlikely(NULL == wraprec || NULL == zf)) {
    return false;
  }

  nr_id = &wraprec->id;

  /* cast zend_function to nruserfn_metadata - cast does not make 
   * any memory allocation but makes the code more readable */
  zf_as_id(&zf_id, zf);

  if (nr_id->lineno != zf_id.lineno) {
    return false;
  }

  if (nr_id->function_name.len != zf_id.function_name.len) {
    return false;
  }

  if (nr_id->filename.len != zf_id.filename.len) {
    return false;
  }

  if (nr_id->scope.len != zf_id.scope.len) {
    return false;
  }

  if (!wraprec_streq(&nr_id->function_name, &zf_id.function_name)) {
    return false;
  }

  if (!wraprec_streq(&nr_id->filename, &zf_id.filename)) {
    return false;
  }

  /* No need to check scope - function has the same name is at the same location in the same file */
  return true;
}

static inline size_t zf2hash(const nr_php_wraprec_hashmap_t* hashmap, const zend_function* zf) {
  size_t hash = nr_php_zend_function_lineno(zf);

  return hash % hashmap->hash_modulo;
}

static inline nruserfn_t* wraprec_hashmap_fetch(unsigned *n, const nr_php_wraprec_hashmap_t* hashmap, const size_t hash, const zend_function* zf) {
  nr_wraprec_hashmap_bucket_t* bucket;

  for (bucket = hashmap->buckets[hash]; bucket; bucket = bucket->next) {
    (*n)++;
    if (wraprec_metadata_is_match(bucket->wraprec, zf)) {
      return bucket->wraprec;
    }
  }

  return NULL;
}

static inline nruserfn_t* wraprec_hashmap_get(unsigned *n, const nr_php_wraprec_hashmap_t* hashmap, const zend_function* zf) {
  size_t hash;

  if (nrunlikely((NULL == hashmap) || (NULL == zf))) {
    return NULL;
  }

  hash = zf2hash(hashmap, zf);
  return wraprec_hashmap_fetch(n, hashmap, hash, zf);
}

static inline nr_status_t wraprec_hashmap_set(nr_php_wraprec_hashmap_t* hashmap, nruserfn_t* wraprec, const zend_function* zf) {
  size_t hash;
  nr_wraprec_hashmap_bucket_t *bucket;
  unsigned n = 0;

  if (nrunlikely((NULL == hashmap) || (NULL == wraprec) || (NULL == zf))) {
    return NR_FAILURE;
  }

  hash = zf2hash(hashmap, zf);
  if (wraprec_hashmap_fetch(&n, hashmap, hash, zf)) {
    /* Maybe worth adding log message as this should not happen? */
    return NR_FAILURE;
  }

  /* Copy zend_function's metadata to nruserfn_metadata. Memory must be freed */
  wraprec_metadata_set(&wraprec->id, zf);

  bucket = (nr_wraprec_hashmap_bucket_t*)nr_malloc(sizeof(nr_wraprec_hashmap_bucket_t));
  bucket->prev = NULL;
  bucket->next = hashmap->buckets[hash];
  bucket->wraprec = wraprec;

  if (hashmap->buckets[hash]) {
    hashmap->buckets[hash]->prev = bucket;
  }

  hashmap->buckets[hash] = bucket;
  ++hashmap->elements;

  return NR_SUCCESS;
}
#endif