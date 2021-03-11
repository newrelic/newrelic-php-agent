/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_file_get_contents.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static int nr_php_file_get_contents_header_search(zval* element,
                                                  char** header_ptr,
                                                  zend_hash_key* key NRUNUSED
                                                      TSRMLS_DC) {
  char* full_header;
  char* header;

  NR_UNUSED_TSRMLS;

  if ((NULL == header_ptr) || (NULL != *header_ptr)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if ((!nr_php_is_zval_valid_string(element))
      || (Z_STRLEN_P(element) < (nr_string_len_t)sizeof(X_NEWRELIC_APP_DATA))
      || (0
          != nr_strnicmp(Z_STRVAL_P(element), NR_PSTR(X_NEWRELIC_APP_DATA)))) {
    return ZEND_HASH_APPLY_KEEP;
  }

  full_header = nr_strndup(Z_STRVAL_P(element), Z_STRLEN_P(element));
  header = nr_header_extract_encoded_value(X_NEWRELIC_APP_DATA, full_header);
  nr_free(full_header);
  *header_ptr = header;

  /*
   * Here we return ZEND_HASH_APPLY_KEEP rather than ZEND_HASH_APPLY_STOP.  In
   * PHP 5.1, returning  ZEND_HASH_APPLY_STOP will remove the element from the
   * hash.  Since the response header hash been acquired, it does not really
   * matter if it gets removed from $http_response_header.  However, when
   * trying to debug cross process, a correct $http_response_header is of great
   * value.
   */
  return ZEND_HASH_APPLY_KEEP;
}

char* nr_php_file_get_contents_response_header(TSRMLS_D) {
  zval* header_zv;
  char* header = NULL;

  if ((0 == nr_php_recording(TSRMLS_C))
      || (0 == NRPRG(txn)->options.cross_process_enabled)) {
    return 0;
  }

  header_zv = nr_php_get_active_php_variable("http_response_header" TSRMLS_CC);
  if (!nr_php_is_zval_valid_array(header_zv)) {
    return 0;
  }

  nr_php_zend_hash_zval_apply(
      Z_ARRVAL_P(header_zv),
      (nr_php_zval_apply_t)nr_php_file_get_contents_header_search,
      &header TSRMLS_CC);

  return header;
}

static void nr_php_file_get_contents_add_headers_internal(zval* context,
                                                          zval* context_options,
                                                          char* headers
                                                              TSRMLS_DC) {
  zval* http_context_options = 0;
  zval* http_header = 0;

  if (NULL == headers) {
    return;
  }
  if (!nr_php_is_zval_valid_array(context_options)) {
    return;
  }

  http_context_options
      = nr_php_zend_hash_find(Z_ARRVAL_P(context_options), "http");
  if (NULL == http_context_options) {
    zval* header_string_zval = nr_php_zval_alloc();
    zval* http_string_zval = nr_php_zval_alloc();
    zval* buf_string_zval = nr_php_zval_alloc();
    zval* retval = NULL;

    nr_php_zval_str(header_string_zval, "header");
    nr_php_zval_str(http_string_zval, "http");
    nr_php_zval_str(buf_string_zval, headers);

    retval = nr_php_call(NULL, "stream_context_set_option", context,
                         http_string_zval, header_string_zval, buf_string_zval);

    nr_php_zval_free(&retval);
    nr_php_zval_free(&header_string_zval);
    nr_php_zval_free(&http_string_zval);
    nr_php_zval_free(&buf_string_zval);
    return;
  }

  if (IS_ARRAY != Z_TYPE_P(http_context_options)) {
    return;
  }

  http_header
      = nr_php_zend_hash_find(Z_ARRVAL_P(http_context_options), "header");
  if (NULL == http_header) {
    nr_php_add_assoc_string(http_context_options, "header", headers);
    return;
  }

  if (IS_ARRAY == Z_TYPE_P(http_header)) {
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
    SEPARATE_ARRAY(http_header);
#endif /* PHP >= 7.3 */
    nr_php_add_next_index_string(http_header, headers);
    return;
  }

  /*
   * From here, we're only interested in string headers. Other types are
   * unsupported.
   */
  if (!nr_php_is_zval_valid_string(http_header)) {
    return;
  }

  if ((Z_STRLEN_P(http_header) <= 0) || (0 == Z_STRVAL_P(http_header))) {
    /* No header string to preserve. */
    nr_php_add_assoc_string(http_context_options, "header", headers);
    return;
  }

  if (nr_stridx(Z_STRVAL_P(http_header), W3C_TRACESTATE":") != -1 &&
      nr_stridx(headers, W3C_TRACESTATE":") != -1) {
    /* Distributed Tracing headers already present and we are trying to
       add them again, don't add duplicates. */
    return;
  }

  /* There is a non-empty header string which must be preserved. */
  {
    char* all_headers = NULL;

    /*
     * IMPORTANT: If this format is changed, the removal logic within
     * nr_php_file_get_contents_remove_headers_internal will need to be
     * changed.
     */
    all_headers
        = nr_formatf("%s%.*s", headers, NRSAFELEN(Z_STRLEN_P(http_header)),
                     Z_STRVAL_P(http_header));

    nr_php_add_assoc_string(http_context_options, "header", all_headers);
    nr_free(all_headers);
  }
}

static char* nr_php_file_get_contents_create_outbound_headers(
    nrtxn_t* txn,
    nr_segment_t* segment) {
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* headers = NULL;
  char* header = NULL;
  char* value = NULL;
  size_t i;
  size_t header_count;

  outbound_headers = nr_header_outbound_request_create(txn, segment);

  if (NULL == outbound_headers) {
    return NULL;
  }

  if (txn && txn->special_flags.debug_cat) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='file_get_contents' %s=" NRP_FMT
        " %s=" NRP_FMT,
        X_NEWRELIC_ID,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                      nr_strlen(X_NEWRELIC_ID))),
        X_NEWRELIC_TRANSACTION,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                                      nr_strlen(X_NEWRELIC_TRANSACTION))));
  }

  header_keys = nr_hashmap_keys(outbound_headers);
  header_count = nr_vector_size(header_keys);

  /*
   * IMPORTANT: If this format is changed, the removal logic within
   * nr_php_file_get_contents_remove_headers_internal will need to be
   * changed.
   */
  for (i = 0; i < header_count; i++) {
    char* formatted_header = NULL;
    char* tmp = NULL;

    header = nr_vector_get(header_keys, i);
    value = (char*)nr_hashmap_get(outbound_headers, header, nr_strlen(header));
    formatted_header = nr_formatf("%s: %s\r\n", header, value);
    tmp = headers;
    headers = nr_formatf("%s%s", headers ? headers : "",
                         formatted_header ? formatted_header : "");

    nr_free(formatted_header);
    nr_free(tmp);
  }

  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
  return headers;
}

zval* nr_php_file_get_contents_get_method(zval* context TSRMLS_DC) {
  zval* context_options = NULL;
  zval* http_context_options = NULL;
  zval* method = NULL;

  if (!nr_php_recording(TSRMLS_C)) {
    return NULL;
  }
  if (0 == NRPRG(txn)->options.cross_process_enabled
      && 0 == NRPRG(txn)->options.distributed_tracing_enabled) {
    return NULL;
  }
  if (NULL == context) {
    return NULL;
  }
  if (IS_RESOURCE != Z_TYPE_P(context)) {
    return NULL;
  }

  context_options = nr_php_call(NULL, "stream_context_get_options", context);

  if (NULL == context_options) {
    return NULL;
  }

  http_context_options
      = nr_php_zend_hash_find(Z_ARRVAL_P(context_options), "http");

  nr_php_zval_free(&context_options);
  if (NULL == http_context_options) {
    return NULL;
  }

  method = nr_php_zend_hash_find(Z_ARRVAL_P(http_context_options), "method");

  return method;
}

void nr_php_file_get_contents_add_headers(zval* context,
                                          nr_segment_t* segment TSRMLS_DC) {
  char* headers = NULL;
  zval* context_options = NULL;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }
  if (0 == NRPRG(txn)->options.cross_process_enabled
      && 0 == NRPRG(txn)->options.distributed_tracing_enabled) {
    return;
  }
  if (NULL == context) {
    return;
  }
  if (IS_RESOURCE != Z_TYPE_P(context)) {
    return;
  }

  context_options = nr_php_call(NULL, "stream_context_get_options", context);

  headers
      = nr_php_file_get_contents_create_outbound_headers(NRPRG(txn), segment);
  nr_php_file_get_contents_add_headers_internal(context, context_options,
                                                headers TSRMLS_CC);

  nr_free(headers);
  nr_php_zval_free(&context_options);
}

static int nr_php_file_get_contents_remove_outbound_headers_iterator(
    zval* element,
    void* extra NRUNUSED,
    zend_hash_key* key NRUNUSED TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (!nr_php_is_zval_non_empty_string(element)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  if (nr_strncaseidx(Z_STRVAL_P(element), X_NEWRELIC_ID, Z_STRLEN_P(element))
      >= 0) {
    return ZEND_HASH_APPLY_REMOVE;
  } else if (nr_strncaseidx(Z_STRVAL_P(element), X_NEWRELIC_SYNTHETICS,
                            Z_STRLEN_P(element))
             >= 0) {
    return ZEND_HASH_APPLY_REMOVE;
  } else {
    return ZEND_HASH_APPLY_KEEP;
  }
}

static void nr_php_file_get_contents_remove_headers_internal(
    zval* context_options TSRMLS_DC) {
  zval* http_context_options = NULL;
  zval* http_header = NULL;

  if (!nr_php_is_zval_valid_array(context_options)) {
    return;
  }

  http_context_options
      = nr_php_zend_hash_find(Z_ARRVAL_P(context_options), "http");
  if (!nr_php_is_zval_valid_array(http_context_options)) {
    return;
  }

  http_header
      = nr_php_zend_hash_find(Z_ARRVAL_P(http_context_options), "header");
  if (NULL == http_header) {
    return;
  }

  if (IS_ARRAY == Z_TYPE_P(http_header)) {
    nr_php_zend_hash_zval_apply(
        Z_ARRVAL_P(http_header),
        (nr_php_zval_apply_t)
            nr_php_file_get_contents_remove_outbound_headers_iterator,
        NULL TSRMLS_CC);
    return;
  }

  if (0 == nr_php_is_zval_non_empty_string(http_header)) {
    return;
  }

  /*
   * This code assumes that the New Relic headers are at the beginning of the
   * headers string, and that they are terminated by \n.
   *
   * See:
   *   nr_php_file_get_contents_create_outbound_headers
   *   nr_php_file_get_contents_add_headers_internal
   */
  {
    char* dup = nr_strndup(Z_STRVAL_P(http_header), Z_STRLEN_P(http_header));
    char* iter = dup;

    while (*iter) {
      /* break on line that is not a New Relic header */
      if (0 != nr_strncaseidx(iter, X_NEWRELIC_ID, nr_strlen(X_NEWRELIC_ID))
          && 0
                 != nr_strncaseidx(iter, X_NEWRELIC_TRANSACTION,
                                   nr_strlen(X_NEWRELIC_TRANSACTION))
          && 0
                 != nr_strncaseidx(iter, X_NEWRELIC_SYNTHETICS,
                                   nr_strlen(X_NEWRELIC_SYNTHETICS))
          && 0 != nr_strncaseidx(iter, NEWRELIC, nr_strlen(NEWRELIC))) {
        break;
      }

      /* next line */
      while (*iter && *iter != '\n') {
        iter++;
      }
      if (*iter == '\n') {
        iter++;
      }
    }
    nr_php_add_assoc_string(http_context_options, "header", iter);

    nr_free(dup);
  }
}

void nr_php_file_get_contents_remove_headers(zval* context TSRMLS_DC) {
  zval* context_options = NULL;

  if (!nr_php_recording(TSRMLS_C)) {
    return;
  }
  if (0 == NRPRG(txn)->options.cross_process_enabled
      && 0 == NRPRG(txn)->options.distributed_tracing_enabled) {
    return;
  }
  if (NULL == context) {
    return;
  }
  if (IS_RESOURCE != Z_TYPE_P(context)) {
    return;
  }

  context_options = nr_php_call(NULL, "stream_context_get_options", context);
  nr_php_file_get_contents_remove_headers_internal(context_options TSRMLS_CC);
  nr_php_zval_free(&context_options);
}

nr_status_t nr_php_file_get_contents_recurse_with_context(
    zval* return_value,
    zval* file_zval,
    zval* use_include_path,
    zval* offset,
    zval* maxlen TSRMLS_DC) {
  zval* argv[5];
  int argc;
  zval* rval = NULL;
  zval* context = NULL;
  int created_use_include_path = 0;

  /*
   * If the call to file_get_contents does not have a context parameter,
   * we must make a recursive call with a context so that we have a context
   * to add the cross process headers to.  Instead of using a new context,
   * the default context must be used since that is what file_get_contents
   * uses in the absence of a context parameter.
   *
   */
  context = nr_php_call(NULL, "stream_context_get_default");

  /*
   * Important: I'm not sure how the stream_context_get_default function
   * could fail, but we must check that context is not NULL to prevent an
   * infinite recursion of this wrapper.
   */
  if (NULL == context) {
    return NR_FAILURE;
  }

  if (NULL == use_include_path) {
    use_include_path = nr_php_zval_alloc();
    nr_php_zval_bool(use_include_path, 0);
    created_use_include_path = 1;
  }

  /*
   * Set up arguments. This has to happen after any arguments may have been
   * modified above.
   */
  argv[0] = file_zval;
  argv[1] = use_include_path;
  argv[2] = context;
  argv[3] = offset;
  argv[4] = maxlen;

  if (maxlen) {
    argc = 5;
  } else if (offset) {
    argc = 4;
  } else {
    argc = 3;
  }

  /*
   * NOTE: This call is not surrounded by a try-catch.  If an exception is
   * thrown, then the created zvals may be leaked.  To my knowledge,
   * file_get_contents cannot throw an exception.  However, this should
   * be reviewed, and we should examine all calls to nr_php_call_user_func
   * for potential exception problems.
   */
  rval = nr_php_call_user_func(NULL, "file_get_contents", argc, argv TSRMLS_CC);

  if (created_use_include_path) {
    nr_php_zval_free(&use_include_path);
  }
  nr_php_zval_free(&context);
  if (rval) {
    ZVAL_ZVAL(return_value, rval, 0, 1);
#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
    /*
     * Calling ZVAL_ZVAL with dtor set to true in PHP 7 won't free the
     * surrounding wrapper.
     */
    efree(rval);
#endif /* PHP7 */
  } else {
    ZVAL_FALSE(return_value);
  }

  return NR_SUCCESS;
}

/* Test scaffolding */
#ifdef TAGS
void zif_newrelic_add_headers_to_context(void); /* ctags landing pad only */
void newrelic_add_headers_to_context(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_add_headers_to_context) {
  zval* context = NULL;
  int rv;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  rv = zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                ZEND_NUM_ARGS() TSRMLS_CC, "z", &context);
  if (SUCCESS != rv) {
    return;
  }
  nr_php_file_get_contents_add_headers(
      context, nr_txn_get_current_segment(NRPRG(txn), NULL) TSRMLS_CC);
}

/* Test scaffolding */
#ifdef TAGS
void zif_newrelic_remove_headers_from_context(
    void);                                       /* ctags landing pad only */
void newrelic_remove_headers_from_context(void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_remove_headers_from_context) {
  zval* context = NULL;
  int rv;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  rv = zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                ZEND_NUM_ARGS() TSRMLS_CC, "z", &context);
  if (SUCCESS != rv) {
    return;
  }
  nr_php_file_get_contents_remove_headers(context TSRMLS_CC);
}
