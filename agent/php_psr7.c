/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_psr7.h"
#include "php_call.h"
#include "php_hash.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"

/*
 * Purpose : Get the URI as a string from a PSR-7 URI object.
 *
 * Params  : 1. The URI object.
 *
 * Returns : The URI, or NULL if the request or URI is invalid.
 */
static char* nr_php_psr7_uri_to_string(zval* uri TSRMLS_DC);

extern char* nr_php_psr7_message_get_header(zval* message,
                                            const char* name TSRMLS_DC) {
  size_t count;
  const zval* header = NULL;
  zval* headers = NULL;
  zval* name_arg = NULL;
  char* value = NULL;

  if ((NULL == name) || !nr_php_psr7_is_message(message TSRMLS_CC)) {
    return NULL;
  }

  name_arg = nr_php_zval_alloc();
  nr_php_zval_str(name_arg, name);

  headers = nr_php_call(message, "getHeader", name_arg);
  if (!nr_php_is_zval_valid_array(headers)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: headers are not an array", __func__);
    goto leave;
  }

  count = nr_php_zend_hash_num_elements(Z_ARRVAL_P(headers));
  if (0 == count) {
    goto leave;
  }

  header = nr_php_zend_hash_index_find(Z_ARRVAL_P(headers), count - 1);
  if (!nr_php_is_zval_valid_string(header)) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: invalid header value", __func__);
    goto leave;
  }

  value = nr_strndup(Z_STRVAL_P(header), Z_STRLEN_P(header));

leave:
  nr_php_zval_free(&headers);
  nr_php_zval_free(&name_arg);

  return value;
}

extern char* nr_php_psr7_request_uri(zval* request TSRMLS_DC) {
  char* uri;
  zval* uri_obj = NULL;

  if (!nr_php_psr7_is_request(request TSRMLS_CC)) {
    return NULL;
  }

  uri_obj = nr_php_call(request, "getUri");
  uri = nr_php_psr7_uri_to_string(uri_obj TSRMLS_CC);
  nr_php_zval_free(&uri_obj);

  return uri;
}

static char* nr_php_psr7_uri_to_string(zval* uri TSRMLS_DC) {
  char* str = NULL;
  zval* zv;

  if (!nr_php_psr7_is_uri(uri TSRMLS_CC)) {
    return NULL;
  }

  zv = nr_php_call(uri, "__toString");
  if (nr_php_is_zval_valid_string(zv)) {
    str = nr_strndup(Z_STRVAL_P(zv), Z_STRLEN_P(zv));
  }

  nr_php_zval_free(&zv);
  return str;
}
