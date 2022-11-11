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
  hashmap->hash_modulo = 53; // or 97 or other prime number
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

static inline void wraprec_metadata_set(nruserfn_t* wraprec, const zend_function* zf) {
    const char* filename = nr_php_function_filename(zf);
  /*
   * Before setting a filename, ensure it is not NULL and it doesn't equal the
   * "special" designation given to funcs without filenames. If the function is
   * an evaluated expression or called directly from the CLI there is no
   * filename, but the function says the filename is "-". Avoid setting in this
   * case; otherwise, all the evaluated/cli calls would match.
   */
  if ((NULL == wraprec->filename) && (NULL != filename)
      && (0 != nr_strcmp("-", filename))) {
    wraprec->filename = nr_strdup(filename);
  }

  wraprec->lineno = nr_php_zend_function_lineno(zf);

  if (chk_reported_class(zf, wraprec)) {
    wraprec->reportedclass = nr_strdup(ZSTR_VAL(zf->common.scope->name));
  }
}

static inline bool wraprec_metadata_is_match(const nruserfn_t* wraprec, const zend_function* zf) {
  char* klass = NULL;
  const char* filename = NULL;

  /*
   * We are able to match either by lineno/filename pair or funcname/classname
   * pair.
   */

  /*
   * Optimize out string manipulations; don't do them if you don't have to.
   * For instance, if funcname doesn't match, no use comparing the classname.
   */

  if (NULL == wraprec) {
    return false;
  }
  if ((NULL == zf) || (ZEND_USER_FUNCTION != zf->type)) {
    return false;
  }

  if (0 != wraprec->lineno) {
    /*
     * Lineno is set in the wraprec.  If lineno doesn't match, we can exit without
     * going on to the funcname/classname pair comparison.
     * If lineno matches, but the wraprec filename is NULL, it is inconclusive and we
     * we must do the funcname/classname compare.
     * If lineno matches, wraprec filename is not NULL, and it matches/doesn't match,
     * we can exit without doing the funcname/classname compare.
     */
    if (wraprec->lineno != nr_php_zend_function_lineno(zf)) {
      return false;
    } 
    /*
     * lineno matched, let's check the filename
     */
    filename = nr_php_function_filename(zf);

    /*
     * If p->filename isn't NULL, we know the comparison is accurate;
     * otherwise, it's inconclusive even if we have a lineno because it
     * could be a cli call or evaluated expression that has no filename.
     */
    if (NULL != wraprec->filename) {
      if (0 == nr_strcmp(wraprec->filename, filename)) {
        return true;
      }
      return false;
    }
  }

  if (NULL == zf->common.function_name) {
    return false;
  }

  if (0 != nr_stricmp(wraprec->funcnameLC, ZSTR_VAL(zf->common.function_name))) {
    return false;
  }
  if (NULL != zf->common.scope && NULL != zf->common.scope->name) {
    klass = ZSTR_VAL(zf->common.scope->name);
  }

  if ((0 == nr_strcmp(wraprec->reportedclass, klass))
      || (0 == nr_stricmp(wraprec->classname, klass))) {
#if 0
    /*
     * If we get here it means lineno/filename weren't initially set.
     * Set it now so we can do the optimized compare next time.
     * lineno/filename is usually not set if the func wasn't loaded when we
     * created the initial wraprec and we had to use the more difficult way to
     * set, update it with lineno/filename now.
     */
    if (NULL == wraprec->filename) {
      filename = nr_php_function_filename(zf);
      if ((NULL != filename) && (0 != nr_strcmp("-", filename))) {
        wraprec->filename = nr_strdup(filename);
      }
    }
    if (0 == wraprec->lineno) {
      wraprec->lineno = nr_php_zend_function_lineno(zf);
    }
#endif
    return true;
  }
  return false;

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

  wraprec_metadata_set(wraprec, zf);

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