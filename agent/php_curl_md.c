/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_curl_md.h"
#include "util_logging.h"

static void nr_php_curl_md_destroy(nr_php_curl_md_t* metadata) {
  nr_php_zval_free(&metadata->outbound_headers);
  nr_free(metadata->method);
  nr_free(metadata->response_header);
  nr_free(metadata);
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP 8.0+ */
#define check_curl_handle(ch)                                              \
  ({                                                                       \
    bool __ok = true;                                                      \
                                                                           \
    if (nrunlikely(!nr_php_is_zval_valid_object(ch))) {                    \
      nrl_verbosedebug(NRL_CAT, "%s: invalid curl handle; not an object",  \
                       __func__);                                          \
      __ok = false;                                                        \
    }                                                                      \
                                                                           \
    __ok;                                                                  \
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

#define ensure_curl_metadata_hashmap()                                       \
  if (!NRTXNGLOBAL(curl_metadata)) {                                         \
    NRTXNGLOBAL(curl_metadata)                                               \
        = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_php_curl_md_destroy); \
  }

#define ensure_curl_multi_metadata_hashmap()                   \
  if (!NRTXNGLOBAL(curl_multi_metadata)) {                     \
    NRTXNGLOBAL(curl_multi_metadata) = nr_hashmap_create(      \
        (nr_hashmap_dtor_func_t)nr_php_curl_multi_md_destroy); \
  }

#define get_curl_metadata(ch)                                             \
  ({                                                                      \
    uint64_t __id = (uint64_t)nr_php_zval_resource_id(ch);                \
    nr_php_curl_md_t* __metadata = NULL;                                  \
                                                                          \
    ensure_curl_metadata_hashmap();                                       \
                                                                          \
    __metadata = nr_hashmap_index_get(NRTXNGLOBAL(curl_metadata), __id);  \
    if (!__metadata) {                                                    \
      __metadata = nr_zalloc(sizeof(nr_php_curl_md_t));                   \
      nr_hashmap_index_set(NRTXNGLOBAL(curl_metadata), __id, __metadata); \
    }                                                                     \
                                                                          \
    __metadata;                                                           \
  })

#define get_curl_multi_metadata(mh)                                     \
  ({                                                                    \
    uint64_t __id = (uint64_t)nr_php_zval_resource_id(mh);              \
    nr_php_curl_multi_md_t* __multi_metadata;                           \
    size_t async_index;                                                 \
                                                                        \
    ensure_curl_multi_metadata_hashmap();                               \
                                                                        \
    __multi_metadata                                                    \
        = nr_hashmap_index_get(NRTXNGLOBAL(curl_multi_metadata), __id); \
                                                                        \
    if (!__multi_metadata) {                                            \
      __multi_metadata = nr_zalloc(sizeof(nr_php_curl_multi_md_t));     \
      nr_hashmap_index_set(NRTXNGLOBAL(curl_multi_metadata), __id,      \
                           __multi_metadata);                           \
      async_index = nr_hashmap_count(NRTXNGLOBAL(curl_multi_metadata)); \
      if (!nr_php_curl_multi_md_init(__multi_metadata, async_index)) {  \
        nr_free(__multi_metadata);                                      \
      }                                                                 \
    }                                                                   \
    __multi_metadata;                                                   \
  })

static int curl_handle_comparator(const void* a,
                                  const void* b,
                                  void* userdata NRUNUSED) {
  int resource_a = 0;
  int resource_b = 0;
  const zval* za = (const zval*)a;
  const zval* zb = (const zval*)b;

  if (za) {
    resource_a = nr_php_zval_resource_id(za);
  }

  if (zb) {
    resource_b = nr_php_zval_resource_id(zb);
  }

  if (resource_a < resource_b) {
    return -1;
  } else if (resource_a > resource_b) {
    return 1;
  } else {
    return 0;
  }
}

const nr_php_curl_md_t* nr_php_curl_md_get(const zval* ch TSRMLS_DC) {
  if (!check_curl_handle(ch)) {
    return NULL;
  }

  return get_curl_metadata(ch);
}

bool nr_php_curl_md_set_method(const zval* ch, const char* method TSRMLS_DC) {
  nr_php_curl_md_t* metadata;

  if (!check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch);
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

  nr_php_curl_md_t* metadata = get_curl_metadata(ch);

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

  metadata = get_curl_metadata(ch);
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

  metadata = get_curl_metadata(ch);
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

  nr_php_curl_md_t* metadata = get_curl_metadata(ch);

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

  metadata = get_curl_metadata(ch);
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

  metadata = get_curl_metadata(ch);
  if (nrunlikely(NULL == metadata)) {
    return NULL;
  }

  if (nr_txn_start_time(NRPRG(txn)) != metadata->txn_start_time) {
    return NULL;
  }

  return metadata->segment;
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

static void nr_php_curl_multi_md_destroy(
    nr_php_curl_multi_md_t* multi_metadata) {
  nr_vector_deinit(&multi_metadata->curl_handles);
  nr_free(multi_metadata->async_context);
  nr_free(multi_metadata);
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

  return get_curl_multi_metadata(mh);
}

bool nr_php_curl_multi_md_add(const zval* mh, zval* ch TSRMLS_DC) {
  nr_php_curl_md_t* metadata;
  nr_php_curl_multi_md_t* multi_metadata;
  zval* handle;

  if (!check_curl_handle(mh) || !check_curl_handle(ch)) {
    return false;
  }

  metadata = get_curl_metadata(ch);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl metadata", __func__);
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh);
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

  metadata = get_curl_metadata(ch);
  if (nrunlikely(NULL == metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl metadata", __func__);
    return false;
  }

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
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

  multi_metadata = get_curl_multi_metadata(mh);
  if (nrunlikely(NULL == multi_metadata)) {
    nrl_error(NRL_CAT, "%s: error creating curl_multi metadata", __func__);
    return false;
  }

  return multi_metadata->initialized;
}
