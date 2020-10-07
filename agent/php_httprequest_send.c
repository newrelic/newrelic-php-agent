/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_httprequest_send.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"

/*
 * This pecl_http 1 instrumentation is currently not supported for Distributed
 * Tracing.
 */
void nr_php_httprequest_send_request_headers(zval* this_var,
                                             nr_segment_t* segment TSRMLS_DC) {
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* value = NULL;
  zval* arr = NULL;
  zval* retval = NULL;
  size_t i;
  size_t header_count;

  if ((NULL == this_var) || (0 == NRPRG(txn)->options.cross_process_enabled)) {
    return;
  }

  outbound_headers = nr_header_outbound_request_create(NRPRG(txn), segment);

  if (NULL == outbound_headers) {
    return;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='pecl_http 1' %s=" NRP_FMT
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
  arr = nr_php_zval_alloc();
  array_init(arr);

  for (i = 0; i < header_count; i++) {
    header = nr_vector_get(header_keys, i);
    value = (char*)nr_hashmap_get(outbound_headers, header, nr_strlen(header));
    nr_php_add_assoc_string(arr, header, value);
  }

  retval = nr_php_call(this_var, "addHeaders", arr);

  nr_php_zval_free(&arr);
  nr_php_zval_free(&retval);
  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}

char* nr_php_httprequest_send_response_header(zval* this_var TSRMLS_DC) {
  zval* retval = NULL;
  zval* header_name = NULL;
  char* x_newrelic_app_data = NULL;

  if ((NULL == this_var) || (0 == NRPRG(txn)->options.cross_process_enabled)) {
    return NULL;
  }

  header_name = nr_php_zval_alloc();
  /*
   * Although we use the lower case name here, it doesn't matter since
   * getResponseHeader will transform the string into the proper format.
   */
  nr_php_zval_str(header_name, X_NEWRELIC_APP_DATA_LOWERCASE);

  retval = nr_php_call(this_var, "getResponseHeader", header_name);
  if (nr_php_is_zval_non_empty_string(retval)) {
    x_newrelic_app_data = nr_strndup(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
  }

  nr_php_zval_free(&header_name);
  nr_php_zval_free(&retval);

  return x_newrelic_app_data;
}

uint64_t nr_php_httprequest_send_response_code(zval* this_var TSRMLS_DC) {
  zval* codez = NULL;
  uint64_t code = 0;

  if (NULL == this_var) {
    return 0;
  }

  codez = nr_php_call(this_var, "getResponseCode");
  if (nr_php_is_zval_valid_integer(codez)) {
    code = Z_LVAL_P(codez);
  }

  nr_php_zval_free(&codez);

  return code;
}

char* nr_php_httprequest_send_get_url(zval* this_var TSRMLS_DC) {
  char* url = NULL;
  zval* urlz = NULL;

  if (NULL == this_var) {
    return NULL;
  }

  urlz = nr_php_call(this_var, "getUrl");
  if (nr_php_is_zval_non_empty_string(urlz)) {
    url = nr_strndup(Z_STRVAL_P(urlz), Z_STRLEN_P(urlz));
  }

  nr_php_zval_free(&urlz);

  return url;
}
