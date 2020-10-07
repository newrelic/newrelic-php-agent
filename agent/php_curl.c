/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_curl.h"
#include "php_curl_md.h"
#include "php_hash.h"
#include "php_includes.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_strings.h"

#include "lib_guzzle_common.h"

static int nr_php_curl_do_cross_process(TSRMLS_D) {
  if (0 == nr_php_recording(TSRMLS_C)) {
    return 0;
  }
  return (NRPRG(txn)->options.cross_process_enabled
          || NRPRG(txn)->options.distributed_tracing_enabled);
}

static void nr_php_curl_save_response_header_from_zval(const zval* ch,
                                                       const zval* zstr
                                                           TSRMLS_DC) {
  char* hdr = NULL;

  if (!nr_php_is_zval_non_empty_string(zstr)) {
    return;
  }

  if (!nr_php_curl_do_cross_process(TSRMLS_C)) {
    return;
  }

  hdr = nr_header_extract_encoded_value(X_NEWRELIC_APP_DATA, Z_STRVAL_P(zstr));
  if (NULL == hdr) {
    return;
  }

  nr_php_curl_md_set_response_header(ch, hdr TSRMLS_CC);
  nr_free(hdr);
}

/*
 * This wrapper should be attached to any function which has been set as
 * callback to receive curl_exec headers (set using curl_setopt). The callback
 * is expected to have two parameters: The curl resource and a string containing
 * header data.
 */
NR_PHP_WRAPPER(nr_php_curl_user_header_callback) {
  zval* ch = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  zval* headers = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  (void)wraprec;

  nr_php_curl_save_response_header_from_zval(ch, headers TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_php_arg_release(&ch);
  nr_php_arg_release(&headers);
}
NR_PHP_WRAPPER_END

#define NR_CURL_RESPONSE_HEADER_CALLBACK_NAME "newrelic_curl_header_callback"
#ifdef TAGS
void zif_newrelic_curl_header_callback(void); /* ctags landing pad only */
void newrelic_curl_header_callback(void);     /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_curl_header_callback) {
  zval* curl_resource = NULL;
  zval* header_data = NULL;
  int rv;

  NR_UNUSED_HT;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  rv = zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                ZEND_NUM_ARGS() TSRMLS_CC, "zz", &curl_resource,
                                &header_data);

  /*
   * This callback is expected to return the length of the header_data received.
   */
  if (nr_php_is_zval_non_empty_string(header_data)) {
    RETVAL_LONG(Z_STRLEN_P(header_data));
  } else {
    RETVAL_LONG(0);
  }

  if (SUCCESS != rv) {
    return;
  }

  nr_php_curl_save_response_header_from_zval(curl_resource,
                                             header_data TSRMLS_CC);
}

static void nr_php_curl_set_default_response_header_callback(
    zval* curlres TSRMLS_DC) {
  zval* callback_name = NULL;
  zval* retval = NULL;
  zval* curlopt = NULL;

  if ((NULL == curlres) || (IS_RESOURCE != Z_TYPE_P(curlres))) {
    return;
  }

  curlopt = nr_php_get_constant("CURLOPT_HEADERFUNCTION" TSRMLS_CC);
  if (NULL == curlopt) {
    return;
  }

  callback_name = nr_php_zval_alloc();
  nr_php_zval_str(callback_name, NR_CURL_RESPONSE_HEADER_CALLBACK_NAME);

  retval = nr_php_call(NULL, "curl_setopt", curlres, curlopt, callback_name);

  nr_php_zval_free(&retval);
  nr_php_zval_free(&callback_name);
  nr_php_zval_free(&curlopt);
}

static void nr_php_curl_set_default_request_headers(zval* curlres TSRMLS_DC) {
  zval* arr = NULL;
  zval* retval = NULL;
  zval* curlopt = NULL;

  if ((NULL == curlres) || (IS_RESOURCE != Z_TYPE_P(curlres))) {
    return;
  }

  curlopt = nr_php_get_constant("CURLOPT_HTTPHEADER" TSRMLS_CC);
  if (NULL == curlopt) {
    return;
  }

  arr = nr_php_zval_alloc();
  array_init(arr);
  /*
   * Note that we do not need to populate the 'arr' parameter with the
   * New Relic headers as those are added by the curl_setopt instrumentation.
   */

  retval = nr_php_call(NULL, "curl_setopt", curlres, curlopt, arr);

  nr_php_zval_free(&retval);
  nr_php_zval_free(&arr);
  nr_php_zval_free(&curlopt);
}

void nr_php_curl_init(zval* curlres TSRMLS_DC) {
  if (0 == nr_php_curl_do_cross_process(TSRMLS_C)) {
    return;
  }

  nr_php_curl_set_default_response_header_callback(curlres TSRMLS_CC);
  nr_php_curl_set_default_request_headers(curlres TSRMLS_CC);
}

static inline bool nr_php_curl_header_contains(const char* haystack,
                                               nr_string_len_t len,
                                               const char* needle) {
  return nr_strncaseidx(haystack, needle, len) >= 0;
}

static inline bool nr_php_curl_header_is_newrelic(const zval* element) {
  nr_string_len_t len;
  const char* val = NULL;

  if (!nr_php_is_zval_valid_string(element)) {
    return false;
  }

  val = Z_STRVAL_P(element);
  len = Z_STRLEN_P(element);

  return nr_php_curl_header_contains(val, len, X_NEWRELIC_ID)
         || nr_php_curl_header_contains(val, len, X_NEWRELIC_TRANSACTION)
         || nr_php_curl_header_contains(val, len, X_NEWRELIC_SYNTHETICS)
         || nr_php_curl_header_contains(val, len, NEWRELIC);
}

static inline void nr_php_curl_copy_header_value(zval* dest, zval* element) {
  /*
   * Copy the header into the destination array, being careful to increment the
   * refcount on the element to avoid double frees.
   */
#ifdef PHP7
  if (Z_REFCOUNTED_P(element)) {
    Z_ADDREF_P(element);
  }
#else
  Z_ADDREF_P(element);
#endif
  add_next_index_zval(dest, element);
}

/*
 * Purpose : Add the New Relic headers to the request. If the user added
 *           headers using curl_setopt they will have been saved in
 *           curl_headers and will be added as well.
 *
 * Params : 1. Curl Resource
 *          2. The current segment.
 */
static void nr_php_curl_exec_set_httpheaders(zval* curlres,
                                             nr_segment_t* segment TSRMLS_DC) {
  zval* headers = NULL;
  int old_curl_ignore_setopt = NRTXNGLOBAL(curl_ignore_setopt);
  zval* retval = NULL;
  zval* curlopt = NULL;
  const nr_php_curl_md_t* metadata = NULL;
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* value = NULL;
  char* formatted_header = NULL;
  size_t i;
  size_t header_count;

  /*
   * Although there's a check further down in
   * nr_header_outbound_request_create(), we can avoid a bunch of work and
   * return early if segment isn't set, since we can't generate a payload
   * regardless.
   */
  if (NULL == segment) {
    return;
  }

  /*
   * If CAT and DT are disabled, user headers are not cached but left in
   * place. Therefore there's nothing to do.
   */
  if (!nr_php_curl_do_cross_process(TSRMLS_C)) {
    return;
  }

  metadata = nr_php_curl_md_get(curlres TSRMLS_CC);
  if (!metadata) {
    nrl_warning(NRL_CAT,
                "Could not instrument curl handle, it may have been "
                "initialized in a different transaction.");
    return;
  }

  /*
   * Set up a new array that we can modify if needed to invoke curl_setopt()
   * with any New Relic headers we need to add.
   */
  headers = nr_php_zval_alloc();
  array_init(headers);

  /*
   * If there are saved headers, we need to ensure that we add them.
   */
  if (nr_php_is_zval_valid_array(metadata->outbound_headers)) {
    zend_ulong key_num = 0;
    nr_php_string_hash_key_t* key_str = NULL;
    zval* val = NULL;

    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(metadata->outbound_headers), key_num,
                              key_str, val) {
      /*
       * If a New Relic header is already present in the header array, that
       * means a higher level piece of instrumentation has added headers already
       * and we don't need to do anything here: let's just get out.
       */
      if (nr_php_curl_header_is_newrelic(val)) {
        goto end;
      }

      /*
       * As curl header arrays are always numerically-indexed, we don't need to
       * preserve the key, and therefore don't look at the variables.
       */
      (void)key_num;
      (void)key_str;

      nr_php_curl_copy_header_value(headers, val);
    }
    ZEND_HASH_FOREACH_END();
  }

  /*
   * OK, there were no New Relic headers (otherwise we'd already have jumped in
   * the loop above). So let's generate some headers, and we can add them to
   * the request.
   */
  outbound_headers = nr_header_outbound_request_create(NRPRG(txn), segment);

  if (NULL == outbound_headers) {
    goto end;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='curl' %s=" NRP_FMT " %s=" NRP_FMT,
        X_NEWRELIC_ID,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_ID,
                                      nr_strlen(X_NEWRELIC_ID))),
        X_NEWRELIC_TRANSACTION,
        NRP_CAT((char*)nr_hashmap_get(outbound_headers, X_NEWRELIC_TRANSACTION,
                                      nr_strlen(X_NEWRELIC_TRANSACTION))));
  }

  header_keys = nr_hashmap_keys(outbound_headers);
  header_count = nr_vector_size(header_keys);

  for (i = 0; i < header_count; i++) {
    header = nr_vector_get(header_keys, i);
    value = (char*)nr_hashmap_get(outbound_headers, header, nr_strlen(header));
    formatted_header = nr_header_format_name_value(header, value, 0);
    nr_php_add_next_index_string(headers, formatted_header);

    nr_free(formatted_header);
  }

  /*
   * Call curl_setopt() with the modified headers, taking care to set the
   * curl_ignore_setopt flag to avoid infinite recursion.
   */
  curlopt = nr_php_get_constant("CURLOPT_HTTPHEADER" TSRMLS_CC);
  if (NULL == curlopt) {
    goto end;
  }

  NRTXNGLOBAL(curl_ignore_setopt) = 1;
  retval = nr_php_call(NULL, "curl_setopt", curlres, curlopt, headers);
  if (!nr_php_is_zval_true(retval)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: error calling curl_setopt", __func__);
  }

  /*
   * We're done. Whether we were successful or not, let's clean up and return.
   */
  NRTXNGLOBAL(curl_ignore_setopt) = old_curl_ignore_setopt;

end:
  nr_php_zval_free(&headers);
  nr_php_zval_free(&retval);
  nr_php_zval_free(&curlopt);
  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}

static void nr_php_curl_setopt_curlopt_writeheader(zval* curlval TSRMLS_DC) {
  if ((NULL == curlval) || (IS_RESOURCE != Z_TYPE_P(curlval))) {
    return;
  }

  /*
   * The user is setting a file to get the response headers.  This use case is
   * not currently supported.  Unfortunately, there does not seem to be any
   * simple solution: Adding a filter to the file stream may not work because
   * the filter could be applied long after the curl_exec call has finished.
   */
  nrm_force_add(NRPRG(txn) ? NRPRG(txn)->unscoped_metrics : 0,
                "Supportability/Unsupported/curl_setopt/CURLOPT_WRITEHEADER",
                0);
}

static void nr_php_curl_setopt_curlopt_headerfunction(zval* curlval TSRMLS_DC) {
  const char* our_callback = NR_CURL_RESPONSE_HEADER_CALLBACK_NAME;
  nr_string_len_t our_callback_len
      = sizeof(NR_CURL_RESPONSE_HEADER_CALLBACK_NAME) - 1;

  if (NULL == curlval) {
    return;
  }

  if (nr_php_is_zval_valid_object(curlval)) {
    /*
     * Here the user could be setting the callback function to an anonymous
     * closure.  This case is not yet supported.
     */
    nrm_force_add(
        NRPRG(txn) ? NRPRG(txn)->unscoped_metrics : 0,
        "Supportability/Unsupported/curl_setopt/CURLOPT_HEADERFUNCTION/closure",
        0);
    return;
  }

  if (!nr_php_is_zval_valid_string(curlval)) {
    return;
  }

  if ((our_callback_len == Z_STRLEN_P(curlval))
      && (0
          == nr_strncmp(our_callback, Z_STRVAL_P(curlval), our_callback_len))) {
    /*
     * Abort if curl_setopt is being used to set our callback function as the
     * function to receive the response headers.  Note that we cannot put a
     * wraprec around our callback to gather the response header because our
     * callback is an internal function.
     */
    return;
  }

  nr_php_wrap_user_function(Z_STRVAL_P(curlval), (size_t)Z_STRLEN_P(curlval),
                            nr_php_curl_user_header_callback TSRMLS_CC);
}

void nr_php_curl_setopt_pre(zval* curlres,
                            zval* curlopt,
                            zval* curlval TSRMLS_DC) {
  if (0 == nr_php_curl_do_cross_process(TSRMLS_C)) {
    return;
  }

  if ((NULL == curlres) || (NULL == curlopt) || (NULL == curlval)
      || (IS_RESOURCE != Z_TYPE_P(curlres)) || (IS_LONG != Z_TYPE_P(curlopt))) {
    return;
  }

  if (nr_php_is_zval_named_constant(curlopt, "CURLOPT_WRITEHEADER" TSRMLS_CC)) {
    nr_php_curl_setopt_curlopt_writeheader(curlval TSRMLS_CC);
    return;
  }

  if (nr_php_is_zval_named_constant(curlopt,
                                    "CURLOPT_HEADERFUNCTION" TSRMLS_CC)) {
    nr_php_curl_setopt_curlopt_headerfunction(curlval TSRMLS_CC);
    return;
  }
}

void nr_php_curl_setopt_post(zval* curlres,
                             zval* curlopt,
                             zval* curlval TSRMLS_DC) {
  if (0 == nr_php_curl_do_cross_process(TSRMLS_C)) {
    return;
  }

  if ((NULL == curlres) || (NULL == curlopt) || (NULL == curlval)
      || (IS_RESOURCE != Z_TYPE_P(curlres)) || (IS_LONG != Z_TYPE_P(curlopt))) {
    return;
  }

  if (nr_php_is_zval_named_constant(curlopt, "CURLOPT_HTTPHEADER" TSRMLS_CC)) {
    /*
     * Save the headers so we can re-apply them along with any CAT or DT
     * headers when curl_exec() is invoked.
     *
     * Note that we do _not_ strip any existing CAT or DT headers; it's
     * possible that code instrumenting libraries built on top of curl (such as
     * Guzzle, with the default handler) will already have added the
     * appropriate headers, so we want to preserve those (since they likely
     * have the correct parent ID).
     */
    if (nr_php_is_zval_valid_array(curlval)) {
      nr_php_curl_md_set_outbound_headers(curlres, curlval TSRMLS_CC);
    } else if (nr_php_object_instanceof_class(curlval,
                                              "Traversable" TSRMLS_CC)) {
      zval* arr = nr_php_call(NULL, "iterator_to_array", curlval);

      nr_php_curl_md_set_outbound_headers(curlres, arr TSRMLS_CC);

      nr_php_zval_free(&arr);
    }
  } else if (nr_php_is_zval_named_constant(curlopt, "CURLOPT_POST" TSRMLS_CC)) {
    nr_php_curl_md_set_method(curlres, "POST" TSRMLS_CC);
  } else if (nr_php_is_zval_named_constant(curlopt, "CURLOPT_PUT" TSRMLS_CC)) {
    nr_php_curl_md_set_method(curlres, "PUT" TSRMLS_CC);
  } else if (nr_php_is_zval_named_constant(curlopt,
                                           "CURLOPT_HTTPGET" TSRMLS_CC)) {
    nr_php_curl_md_set_method(curlres, "GET" TSRMLS_CC);
  } else if (nr_php_is_zval_named_constant(curlopt,
                                           "CURLOPT_CUSTOMREQUEST" TSRMLS_CC)) {
    if (nr_php_is_zval_valid_string(curlval)) {
      nr_php_curl_md_set_method(curlres, Z_STRVAL_P(curlval) TSRMLS_CC);
    }
  }
}

typedef struct _nr_php_curl_setopt_array_apply_t {
  zval* curlres;
  nr_php_curl_setopt_func_t func;
} nr_php_curl_setopt_array_apply_t;

static int nr_php_curl_setopt_array_apply(zval* value,
                                          nr_php_curl_setopt_array_apply_t* app,
                                          zend_hash_key* hash_key TSRMLS_DC) {
  zval* key = nr_php_zval_alloc();

  if (nr_php_zend_hash_key_is_string(hash_key)) {
    nr_php_zval_str(key, nr_php_zend_hash_key_string_value(hash_key));
  } else if (nr_php_zend_hash_key_is_numeric(hash_key)) {
    ZVAL_LONG(key, (zend_long)nr_php_zend_hash_key_integer(hash_key));
  } else {
    /*
     * This is a warning because this really, really shouldn't ever happen.
     */
    nrl_warning(NRL_INSTRUMENT, "%s: unexpected key type", __func__);
    goto end;
  }

  /*
   * Actually invoke the pre/post function.
   */
  (app->func)(app->curlres, key, value TSRMLS_CC);

end:
  nr_php_zval_free(&key);
  return ZEND_HASH_APPLY_KEEP;
}

void nr_php_curl_setopt_array(zval* curlres,
                              zval* options,
                              nr_php_curl_setopt_func_t func TSRMLS_DC) {
  nr_php_curl_setopt_array_apply_t app = {
      .curlres = curlres,
      .func = func,
  };

  if (!nr_php_is_zval_valid_resource(curlres)
      || !nr_php_is_zval_valid_array(options)) {
    return;
  }

  nr_php_zend_hash_zval_apply(
      Z_ARRVAL_P(options), (nr_php_zval_apply_t)nr_php_curl_setopt_array_apply,
      (void*)&app TSRMLS_CC);
}

static bool nr_php_curl_finished(zval* curlres TSRMLS_DC) {
  zval* curlinfo_http_code = NULL;
  zval* result = NULL;
  bool finished = false;

  if (!nr_php_is_zval_valid_resource(curlres)) {
    return false;
  }

  curlinfo_http_code = nr_php_get_constant("CURLINFO_HTTP_CODE" TSRMLS_CC);
  if (!curlinfo_http_code) {
    return false;
  }

  result = nr_php_call(NULL, "curl_getinfo", curlres, curlinfo_http_code);
  if (nr_php_is_zval_valid_integer(result)) {
    finished = (0 != Z_LVAL_P(result));
  };

  nr_php_zval_free(&result);
  nr_php_zval_free(&curlinfo_http_code);

  return finished;
}

char* nr_php_curl_get_url(zval* curlres TSRMLS_DC) {
  zval* retval = NULL;
  char* url = NULL;
  zval* curlinfo_effective_url = NULL;

  /*
   * Note that we do not check cross process enabled here.  The url is used
   * for curl instrumentation regardless of whether or not cross process is
   * enabled.
   */

  curlinfo_effective_url
      = nr_php_get_constant("CURLINFO_EFFECTIVE_URL" TSRMLS_CC);
  if (NULL == curlinfo_effective_url) {
    return NULL;
  }

  retval = nr_php_call(NULL, "curl_getinfo", curlres, curlinfo_effective_url);
  if (nr_php_is_zval_non_empty_string(retval)) {
    url = nr_strndup(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
  }

  nr_php_zval_free(&retval);
  nr_php_zval_free(&curlinfo_effective_url);
  return url;
}

uint64_t nr_php_curl_get_status_code(zval* curlres TSRMLS_DC) {
  zval* retval = NULL;
  uint64_t status = 0;
  zval* curlinfo_http_code = NULL;

  curlinfo_http_code = nr_php_get_constant("CURLINFO_HTTP_CODE" TSRMLS_CC);
  if (NULL == curlinfo_http_code) {
    return 0;
  }

  retval = nr_php_call(NULL, "curl_getinfo", curlres, curlinfo_http_code);
  if (nr_php_is_zval_valid_integer(retval)) {
    status = Z_LVAL_P(retval);
  };

  nr_php_zval_free(&retval);
  nr_php_zval_free(&curlinfo_http_code);
  return status;
}

/*
 * Purpose : Get the total time of a request from a curl resource
 *
 * Returns : The total time the request took, in microseconds. 0 if the
 *           total time could not be obtained.
 */
static nrtime_t nr_php_curl_get_total_time(zval* curlres TSRMLS_DC) {
  zval* retval = NULL;
  nrtime_t total_time = 0;
  zval* curlinfo_total_time = NULL;

  curlinfo_total_time = nr_php_get_constant("CURLINFO_TOTAL_TIME" TSRMLS_CC);
  if (NULL == curlinfo_total_time) {
    return 0;
  }

  retval = nr_php_call(NULL, "curl_getinfo", curlres, curlinfo_total_time);
  if (nr_php_is_zval_valid_double(retval)) {
    total_time = Z_DVAL_P(retval) * NR_TIME_DIVISOR;
  }

  nr_php_zval_free(&retval);
  nr_php_zval_free(&curlinfo_total_time);

  return total_time;
}

/*
 * This function effectively wraps a list of protocols to ignore.
 *
 *   FILE - ignored because use of the FILE protocol does not involve
 *          any network activity, and because the url is a local
 *          filesystem path. The latter is dangerous because it can
 *          lead to an unbounded number of unique external metrics.
 */
int nr_php_curl_should_instrument_proto(const char* url) {
  return 0 != nr_strncmp(url, NR_PSTR("file://"));
}

void nr_php_curl_exec_pre(zval* curlres,
                          nr_segment_t* parent,
                          const char* async_context TSRMLS_DC) {
  nr_segment_t* segment = NULL;
  char* uri = NULL;

  uri = nr_php_curl_get_url(curlres TSRMLS_CC);

  if (nr_php_curl_should_instrument_proto(uri)
      && (0 == nr_guzzle_in_call_stack(TSRMLS_C))) {
    segment = nr_segment_start(NRPRG(txn), parent, async_context);
    nr_php_curl_md_set_segment(curlres, segment TSRMLS_CC);
  }

  /*
   * We need to invoke nr_php_curl_exec_set_httpheaders() regardless of whether
   * segment is NULL to ensure that we re-add any user headers, even if we're
   * not instrumenting this particular call.
   */
  nr_php_curl_exec_set_httpheaders(curlres, segment TSRMLS_CC);

  nr_free(uri);
}

void nr_php_curl_exec_post(zval* curlres, bool duration_from_handle TSRMLS_DC) {
  nr_segment_external_params_t external_params = {.library = "curl"};
  nr_segment_t* segment = NULL;

  segment = nr_php_curl_md_get_segment(curlres TSRMLS_CC);

  if (!segment) {
    return;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT, "CAT: outbound response: transport='curl' %s=" NRP_FMT,
        X_NEWRELIC_APP_DATA,
        NRP_CAT(nr_php_curl_md_get_response_header(curlres TSRMLS_CC)));
  }

  external_params.procedure
      = nr_strdup(nr_php_curl_md_get_method(curlres TSRMLS_CC));
  external_params.uri = nr_php_curl_get_url(curlres TSRMLS_CC);
  external_params.status = nr_php_curl_get_status_code(curlres TSRMLS_CC);
  external_params.encoded_response_header
      = nr_strdup(nr_php_curl_md_get_response_header(curlres TSRMLS_CC));

  if (duration_from_handle) {
    nr_segment_set_timing(segment, segment->start_time,
                          nr_php_curl_get_total_time(curlres TSRMLS_CC));
  }

  nr_segment_external_end(&segment, &external_params);

  nr_free(external_params.uri);
  nr_free(external_params.procedure);
  nr_free(external_params.encoded_response_header);
}

void nr_php_curl_multi_exec_pre(zval* curlres TSRMLS_DC) {
  size_t pos;
  nr_segment_t* segment = NULL;
  zval* handle = NULL;
  nr_vector_t* handles = NULL;
  const char* async_context = NULL;

  if (nr_php_curl_multi_md_is_initialized(curlres TSRMLS_CC)) {
    return;
  }

  /*
   * If this is the first call to curl_multi_exec, the asynchronous root
   * segment has to be initialized.
   *
   * The segment is ended right away, and with every subsequent call to
   * curl_multi_exec, the end time of the segment is updated.
   */
  if (!nr_guzzle_in_call_stack(TSRMLS_C)) {
    segment = nr_segment_start(
        NRPRG(txn), NULL,
        nr_php_curl_multi_md_get_async_context(curlres TSRMLS_CC));
    nr_segment_set_name(segment, "curl_multi_exec");
    nr_php_curl_multi_md_set_segment(curlres, segment TSRMLS_CC);
  }

  /*
   * We need to invoke nr_php_curl_pre() regardless of whether
   * segment is NULL to ensure that we re-add any user headers, even if we're
   * not instrumenting this particular call.
   */
  handles = nr_php_curl_multi_md_get_handles(curlres TSRMLS_CC);
  async_context = nr_php_curl_multi_md_get_async_context(curlres TSRMLS_CC);

  for (pos = 0; pos < nr_vector_size(handles); pos++) {
    handle = nr_vector_get(handles, pos);

    nr_php_curl_exec_pre(handle, segment, async_context TSRMLS_CC);
  }

  nr_php_curl_multi_md_set_initialized(curlres TSRMLS_CC);
}

void nr_php_curl_multi_exec_post(zval* curlres TSRMLS_DC) {
  size_t pos;
  zval* handle = NULL;
  void* removed_handle = NULL;
  nrtime_t start;
  nr_vector_t* handles = NULL;
  nr_segment_t* segment = NULL;

  handles = nr_php_curl_multi_md_get_handles(curlres TSRMLS_CC);

  /*
   * Looping over all handled added to this curl_multi_exec handle. Each
   * handle is checked; if the request represented by the handle is  done and
   * the necessary instrumentation was created, the handle is removed from the
   * vector.
   */
  if (handles) {
    for (pos = 0; pos < nr_vector_size(handles); pos++) {
      handle = nr_vector_get(handles, pos);

      if (!nr_php_curl_finished(handle TSRMLS_CC)) {
        continue;
      }

      nr_php_curl_exec_post(handle, true TSRMLS_CC);

      nr_vector_remove(handles, pos, &removed_handle);
      pos--;
    }
  }

  /*
   * With every call to curl_multi_exec, the duration of the
   * asynchronous root segment is updated.
   */
  segment = nr_php_curl_multi_md_get_segment(curlres TSRMLS_CC);

  if (segment) {
    start = segment->start_time;
    nr_segment_set_timing(segment, start, nr_txn_now_rel(NRPRG(txn)) - start);
  }
}

void nr_php_curl_multi_exec_finalize(zval* curlres TSRMLS_DC) {
  size_t pos;
  zval* handle = NULL;
  void* removed_handle = NULL;
  nr_vector_t* handles = NULL;
  nr_segment_t* segment = NULL;

  handles = nr_php_curl_multi_md_get_handles(curlres TSRMLS_CC);

  if (handles) {
    for (pos = 0; pos < nr_vector_size(handles); pos++) {
      handle = nr_vector_get(handles, pos);

      nr_php_curl_exec_post(handle, false TSRMLS_CC);

      nr_vector_remove(handles, pos, &removed_handle);
      pos--;
    }
  }

  segment = nr_php_curl_multi_md_get_segment(curlres TSRMLS_CC);
  nr_segment_end(&segment);
}
