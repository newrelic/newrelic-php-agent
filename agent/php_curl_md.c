/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_curl_md.h"
#include "php_objtable.h"
#include "util_logging.h"

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
#define check_curl_handle(ch)                                             \
  ({                                                                      \
    bool __ok = true;                                                     \
                                                                          \
    if (nrunlikely(!nr_php_is_zval_valid_object(ch))) {                   \
      nrl_verbosedebug(NRL_CAT, "%s: invalid curl handle; not an object", \
                       __func__);                                         \
      __ok = false;                                                       \
    }                                                                     \
                                                                          \
    __ok;                                                                 \
  })
#else
#define check_curl_handle(ch)                                              \
  ({                                                                       \
    bool __ok = true;                                                      \
                                                                           \
    if (nrunlikely(!nr_php_is_zval_valid_resource(ch))) {                  \
      nrl_verbosedebug(NRL_CAT, "%s: invalid curl handle; not a resource", \
                       __func__);                                          \
      __ok = false;                                                        \
    }                                                                      \
                                                                           \
    __ok;                                                                  \
  })
#endif

static void nr_php_curl_multi_md_destroy(
    nr_php_curl_multi_md_t* multi_metadata) {
  nr_vector_deinit(&multi_metadata->curl_handles);
  nr_free(multi_metadata->async_context);
  nr_free(multi_metadata);
}

static void nr_php_curl_md_destroy(nr_php_curl_md_t* metadata) {
  nr_php_zval_free(&metadata->outbound_headers);
  nr_free(metadata->method);
  nr_free(metadata->response_header);
  nr_free(metadata);
}

static void curl_handle_vector_dtor(void* element, void* userdata NRUNUSED) {
  zval* handle = (zval*)element;

  if (!handle) {
    return;
  }

  nr_php_zval_free(&handle);
}

/*
 * The index parameter is used to initialize an unique async context
 * name for each curl multi handle inside a transaction.
 *
 * This async context name is used to set proper async context names on
 * segments related to this curl_multi handle.
 */
static bool nr_php_curl_multi_md_init(nr_php_curl_multi_md_t* multi_metadata,
                                      size_t index) {
  multi_metadata->async_context = nr_formatf("curl_multi_exec #%zu", index);

  return nr_vector_init(&multi_metadata->curl_handles, 8,
                        curl_handle_vector_dtor, NULL);
}

static void ensure_curl_metadata_hashmap(TSRMLS_D)
{
  if (!NRTXNGLOBAL(curl_metadata)) {
    NRTXNGLOBAL(curl_metadata)
        = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_php_curl_md_destroy);
  }
}

static void ensure_curl_multi_metadata_hashmap(TSRMLS_D)
{
  if (!NRTXNGLOBAL(curl_multi_metadata)) {
    NRTXNGLOBAL(curl_multi_metadata) = nr_hashmap_create(
        (nr_hashmap_dtor_func_t)nr_php_curl_multi_md_destroy);
  }
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
/*
 * The value kept in the weak table for each curl object: the key of the
 * object in the metadata maps.
 */
typedef struct {
  struct _nr_php_curl_metadata_keys_t* keys;
  uint64_t key;
} nr_php_curl_key_t;

struct _nr_php_curl_metadata_keys_t {
  nr_php_objtable_t* table; /* The key of each curl object */
  uint64_t last_key;        /* The last key that was given out */
  nr_vector_t freed;        /* The keys of the objects that were freed since
                               the metadata was last looked at */
  bool closing;             /* Set while the table is destroyed */
  nrtime_t txn_start_time;  /* The transaction that multi_count is for */
  size_t multi_count;       /* The multi handles of that transaction */
};

/*
 * The destructor runs inside the free of the object. It only frees memory and
 * remembers the key: the metadata is cleaned up the next time the agent looks
 * at it, and no call into PHP is made here.
 */
static void nr_php_curl_key_dtor(void* value) {
  nr_php_curl_key_t* key = (nr_php_curl_key_t*)value;

  if (NULL == key) {
    return;
  }

  if (!key->keys->closing) {
    nr_vector_push_back(&key->keys->freed, (void*)(uintptr_t)key->key);
  }

  nr_free(key);
}

void nr_php_curl_metadata_keys_create(void) {
  struct _nr_php_curl_metadata_keys_t* keys
      = nr_zalloc(sizeof(struct _nr_php_curl_metadata_keys_t));

  keys->table = nr_php_objtable_create(nr_php_curl_key_dtor);
  nr_vector_init(&keys->freed, 8, NULL, NULL);
  NRPRG_SHARED(curl_metadata_keys) = keys;
}

void nr_php_curl_metadata_keys_destroy(void) {
  struct _nr_php_curl_metadata_keys_t* keys = NRPRG_SHARED(curl_metadata_keys);

  if (NULL == keys) {
    return;
  }

  keys->closing = true;
  nr_php_objtable_destroy(&keys->table);
  nr_vector_deinit(&keys->freed);
  nr_free(keys);
  NRPRG_SHARED(curl_metadata_keys) = NULL;
}
#endif

/*
 * Purpose : Get the key of a curl handle in the metadata maps.
 *
 * Returns : The key, or 0 if the zval is not a curl handle.
 */
static uint64_t nr_php_curl_metadata_key(const zval* handle) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
  struct _nr_php_curl_metadata_keys_t* keys = NRPRG_SHARED(curl_metadata_keys);
  nr_php_curl_key_t* key;
  uint64_t value;

  if (0 == nr_php_zval_object_id(handle)) {
    return 0;
  }

  if (NULL == keys) {
    return (uint64_t)nr_php_zval_object_id(handle);
  }

  key = (nr_php_curl_key_t*)nr_php_objtable_get(keys->table, handle);
  if (NULL == key) {
    key = nr_zalloc(sizeof(nr_php_curl_key_t));
    key->keys = keys;
    key->key = ++keys->last_key;
    value = key->key;
    nr_php_objtable_set(keys->table, handle, key);
    return value;
  }

  return key->key;
#else
  return (uint64_t)nr_php_zval_resource_id(handle);
#endif
}

/*
 * Purpose : Get the number of the next multi handle of the transaction, which
 *           makes the name of its async context.
 */
static size_t nr_php_curl_next_multi_index(void) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
  struct _nr_php_curl_metadata_keys_t* keys = NRPRG_SHARED(curl_metadata_keys);

  if (keys && NRPRG(txn)) {
    /* The metadata of the earlier transaction is gone: count again. */
    if (keys->txn_start_time != nr_txn_start_time(NRPRG(txn))) {
      keys->txn_start_time = nr_txn_start_time(NRPRG(txn));
      keys->multi_count = 0;
    }
    return ++keys->multi_count;
  }
#endif
  return nr_hashmap_count(NRTXNGLOBAL(curl_multi_metadata));
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
/*
 * Purpose : Clean up the metadata of the curl handles that were freed. A
 *           multi segment that is still open is ended, and the entries go, so
 *           that the metadata of a loop that makes a new handle for each run
 *           does not pile up.
 */
static void nr_php_curl_drain_freed_keys(void) {
  struct _nr_php_curl_metadata_keys_t* keys = NRPRG_SHARED(curl_metadata_keys);

  while (keys && (nr_vector_size(&keys->freed) > 0)) {
    void* removed = NULL;
    uint64_t key;
    nr_php_curl_multi_md_t* multi_metadata = NULL;

    nr_vector_remove(&keys->freed, nr_vector_size(&keys->freed) - 1, &removed);
    key = (uint64_t)(uintptr_t)removed;

    if (NRTXNGLOBAL(curl_multi_metadata)) {
      multi_metadata
          = nr_hashmap_index_get(NRTXNGLOBAL(curl_multi_metadata), key);
    }
    if (multi_metadata) {
      nr_segment_t* segment = multi_metadata->segment;

      if (segment && NRPRG(txn)
          && (nr_txn_start_time(NRPRG(txn))
              == multi_metadata->txn_start_time)) {
        /*
         * Ending a segment makes the async context of its parent the current
         * one. The context that is current now belongs to the caller.
         */
        int old_context = NRTXN(current_async_context);

        nr_segment_end(&segment);
        NRTXN(current_async_context) = old_context;
      }
      /* Releases the handles that were added to the multi handle. */
      nr_hashmap_index_delete(NRTXNGLOBAL(curl_multi_metadata), key);
    }

    if (NRTXNGLOBAL(curl_metadata)) {
      nr_hashmap_index_delete(NRTXNGLOBAL(curl_metadata), key);
    }
  }
}
#endif

static nr_php_curl_md_t* get_curl_metadata(const zval* ch TSRMLS_DC)
{
    nr_php_curl_md_t* metadata = NULL;
    uint64_t id;

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    nr_php_curl_drain_freed_keys();
#endif
    id = nr_php_curl_metadata_key(ch);
    if (0 == id) {
        return NULL;
    }

    ensure_curl_metadata_hashmap(TSRMLS_C);

    metadata = nr_hashmap_index_get(NRTXNGLOBAL(curl_metadata), id);
    if (!metadata) {
      metadata = nr_zalloc(sizeof(nr_php_curl_md_t));
      nr_hashmap_index_set(NRTXNGLOBAL(curl_metadata), id, metadata);
    }

    return metadata;
}

static nr_php_curl_multi_md_t* get_curl_multi_metadata(const zval* mh TSRMLS_DC)
{
    nr_php_curl_multi_md_t* multi_metadata;
    size_t async_index;
    uint64_t id;

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    nr_php_curl_drain_freed_keys();
#endif
    id = nr_php_curl_metadata_key(mh);
    if (0 == id) {
        return NULL;
    }

    ensure_curl_multi_metadata_hashmap(TSRMLS_C);

    multi_metadata = nr_hashmap_index_get(NRTXNGLOBAL(curl_multi_metadata), id);

    if (!multi_metadata) {
      multi_metadata = nr_zalloc(sizeof(nr_php_curl_multi_md_t));
      nr_hashmap_index_set(NRTXNGLOBAL(curl_multi_metadata), id,
                           multi_metadata);
      async_index = nr_php_curl_next_multi_index();
      if (!nr_php_curl_multi_md_init(multi_metadata, async_index)) {
        nr_free(multi_metadata);
      }
    }
    return multi_metadata;
}

static int curl_handle_comparator(const void* a,
                                  const void* b,
                                  void* userdata NRUNUSED) {
  int id_a = 0;
  int id_b = 0;
  const zval* za = (const zval*)a;
  const zval* zb = (const zval*)b;

  if (za) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    id_a = nr_php_zval_object_id(za);
#else
    id_a = nr_php_zval_resource_id(za);
#endif
  }

  if (zb) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
    id_b = nr_php_zval_object_id(zb);
#else
    id_b = nr_php_zval_resource_id(zb);
#endif
  }

  if (id_a < id_b) {
    return -1;
  } else if (id_a > id_b) {
    return 1;
  } else {
    return 0;
  }
}

const nr_php_curl_md_t* nr_php_curl_md_get(const zval* ch TSRMLS_DC) {
  if (!check_curl_handle(ch)) {
    return NULL;
  }

  return get_curl_metadata(ch TSRMLS_CC);
}

bool nr_php_curl_md_set_method(const zval* ch, const char* method TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (!check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl handle metadata", __func__);
    return false;
  }

  nr_free(metadata->method);
  metadata->method = nr_strdup(method);

  return true;
}

const char* nr_php_curl_md_get_method(const zval* ch TSRMLS_DC) {
  if (!check_curl_handle(ch)) {
    return "GET";
  }

  nr_php_curl_md_t* metadata = get_curl_metadata(ch TSRMLS_CC);

  if (nrunlikely(NULL == metadata) || NULL == metadata->method) {
    return "GET";
  }
  return metadata->method;
}

bool nr_php_curl_md_set_outbound_headers(const zval* ch,
                                         zval* headers TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (nrunlikely(!nr_php_is_zval_valid_array(headers))) {
    nrl_verbosedebug(
        NRL_CAT, "%s: cannot set outbound headers from a non-array", __func__);
    return false;
  }

  if (!check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl handle metadata", __func__);
    return false;
  }

  nr_php_zval_free(&metadata->outbound_headers);

  metadata->outbound_headers = nr_php_zval_alloc();
  ZVAL_DUP(metadata->outbound_headers, headers);

  return true;
}

bool nr_php_curl_md_set_response_header(const zval* ch,
                                        const char* header TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (!check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl handle metadata", __func__);
    return false;
  }

  nr_free(metadata->response_header);
  metadata->response_header = nr_strdup(header);

  return true;
}

const char* nr_php_curl_md_get_response_header(const zval* ch TSRMLS_DC) {
  if (!check_curl_handle(ch)) {
    return false;
  }

  nr_php_curl_md_t* metadata = get_curl_metadata(ch TSRMLS_CC);

  if (nrunlikely(NULL == metadata)) {
    return NULL;
  }
  return metadata->response_header;
}

bool nr_php_curl_md_set_segment(zval* ch, nr_segment_t* segment TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (!check_curl_handle(ch)) {
    return false;
  }

  if (NULL == segment) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl handle metadata", __func__);
    return false;
  }

  metadata->segment = segment;
  metadata->txn_start_time = nr_txn_start_time(segment->txn);

  return true;
}

nr_segment_t* nr_php_curl_md_get_segment(const zval* ch TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (!check_curl_handle(ch)) {
    return NULL;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    return NULL;
  }

  if (nr_txn_start_time(NRPRG(txn)) != metadata->txn_start_time) {
    return NULL;
  }

  return metadata->segment;
}

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO /* PHP 7 */
nr_segment_t* nr_php_curl_multi_md_take_segment(const zval* mh) {
  nr_php_curl_multi_md_t* multi_metadata;
  nr_segment_t* segment;

  if (!check_curl_handle(mh)) {
    return NULL;
  }

  multi_metadata = get_curl_multi_metadata(mh);
  if (nrunlikely(NULL == multi_metadata)) {
    return NULL;
  }

  segment = multi_metadata->segment;
  multi_metadata->segment = NULL;

  if (nr_txn_start_time(NRPRG(txn)) != multi_metadata->txn_start_time) {
    return NULL;
  }

  return segment;
}
#endif

nr_segment_t* nr_php_curl_md_take_segment(const zval* ch) {
  nr_php_curl_md_t* metadata;
  nr_segment_t* segment;

  if (!check_curl_handle(ch)) {
    return NULL;
  }

  metadata = get_curl_metadata(ch);
  if (nrunlikely(NULL == metadata)) {
    return NULL;
  }

  segment = metadata->segment;
  metadata->segment = NULL;

  if (nr_txn_start_time(NRPRG(txn)) != metadata->txn_start_time) {
    return NULL;
  }

  return segment;
}

void nr_curl_rshutdown(TSRMLS_D) {
  /*
   * This frees curl multi metadata stored in the transaction.
   *
   * `curl_multi_metadata` contains duplicates of curl handle zvals. If
   * `nr_php_txn_end` is called from the post-deactivate callback, request
   * shutdown functions have already been called; and the Zend VM has already
   * forcefully freed all dangling zvals that are not referenced by the global
   * scope (regardless of their reference count), thus leaving the zvals stored
   * in the curl multi metadata in an "undefined" state. Consequently, freeing
   * the zvals in `nr_php_txn_end` at this stage can result in undefined
   * behavior.
   *
   * Calling this function during the RSHUTDOWN phase ensures that the zvals in
   * `curl_multi_metadata` are cleaned up before Zend winds down the VM and
   * forcefully frees zvals.
   *
   * If `nr_php_txn_end` is called outside the post-deactivate callback,
   * it frees `curl_multi_metadata` by itself.
   */
  if (nrlikely(NRPRG(txn))) {
    nr_hashmap_destroy(&NRTXNGLOBAL(curl_multi_metadata));
  }
}

nr_php_curl_multi_md_t* nr_php_curl_multi_md_get(const zval* mh TSRMLS_DC) {
  if (!check_curl_handle(mh)) {
    return NULL;
  }

  return get_curl_multi_metadata(mh TSRMLS_CC);
}

bool nr_php_curl_multi_md_add(const zval* mh, zval* ch TSRMLS_DC) {
  nr_php_curl_md_t* metadata;
  nr_php_curl_multi_md_t* multi_metadata;
  zval* handle;

  if (!check_curl_handle(mh) || !check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl metadata", __func__);
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl multi metadata", __func__);
    return false;
  }

  size_t index;
  if (nr_vector_find_first(&multi_metadata->curl_handles, ch,
                           curl_handle_comparator, NULL, &index)) {
    nrl_verbosedebug(NRL_CAT, "%s: curl handle already in curl multi metadata",
                     __func__);
    return false;
  }

  handle = nr_php_zval_alloc();
  ZVAL_DUP(handle, ch);
  if (!nr_vector_push_back(&multi_metadata->curl_handles, handle)) {
    nrl_error(NRL_CAT, "%s: error adding curl handle to curl multi metadata",
              __func__);
    nr_php_zval_free(&handle);
    return false;
  }

  return true;
}

bool nr_php_curl_multi_md_remove(const zval* mh, const zval* ch TSRMLS_DC) {
  nr_php_curl_md_t* metadata;
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh) || !check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch TSRMLS_CC);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl metadata", __func__);
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl multi metadata", __func__);
    return false;
  }

  size_t index;
  if (nr_vector_find_first(&multi_metadata->curl_handles, ch,
                           curl_handle_comparator, NULL, &index)) {
    zval* element;
    if (!nr_vector_remove(&multi_metadata->curl_handles, index,
                          (void**)&element)) {
      nrl_error(NRL_CAT, "%s: error removing curl_multi handle metadata",
                __func__);
      return false;
    }
    nr_php_zval_free(&element);
  } else {
    nrl_verbosedebug(
        NRL_CAT, "%s: curl handle not found in curl multi metadata", __func__);
    return false;
  }

  return true;
}

bool nr_php_curl_multi_md_set_segment(zval* mh,
                                      nr_segment_t* segment TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return false;
  }

  if (NULL == segment) {
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi handle metadata",
              __func__);
    return false;
  }

  multi_metadata->segment = segment;
  multi_metadata->txn_start_time = nr_txn_start_time(segment->txn);

  return true;
}

nr_segment_t* nr_php_curl_multi_md_get_segment(const zval* mh TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return NULL;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    return NULL;
  }

  if (nr_txn_start_time(NRPRG(txn)) != multi_metadata->txn_start_time) {
    return NULL;
  }

  return multi_metadata->segment;
}

const char* nr_php_curl_multi_md_get_async_context(const zval* mh TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return NULL;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi metadata", __func__);
    return NULL;
  }

  return multi_metadata->async_context;
}

nr_vector_t* nr_php_curl_multi_md_get_handles(const zval* mh TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return NULL;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi metadata", __func__);
    return NULL;
  }

  return &multi_metadata->curl_handles;
}

bool nr_php_curl_multi_md_set_initialized(const zval* mh TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi metadata", __func__);
    return false;
  }

  multi_metadata->initialized = true;

  return true;
}

bool nr_php_curl_multi_md_is_initialized(const zval* mh TSRMLS_DC) {
  nr_php_curl_multi_md_t* multi_metadata;

  if (!check_curl_handle(mh)) {
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh TSRMLS_CC);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi metadata", __func__);
    return false;
  }

  return multi_metadata->initialized;
}
