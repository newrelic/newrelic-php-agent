/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_output.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#define NR_NONEXISTENT_HEADER "X-New-Relic-Non-Existent-Header"

char* nr_php_get_request_header(const char* name TSRMLS_DC) {
  /*
   * Request headers can be accessed through $_SERVER.
   */
  return nr_php_get_server_global(name TSRMLS_CC);
}

int nr_php_has_request_header(const char* name TSRMLS_DC) {
  char* header = nr_php_get_request_header(name TSRMLS_CC);

  if (NULL == header) {
    return 0;
  }

  nr_free(header);
  return 1;
}

typedef struct _nr_zend_llist_search_t {
  const char* name;
  int namelen;
  char* value;
} nr_zend_llist_search_t;

static void nr_php_get_response_header_search(void* data, void* arg TSRMLS_DC) {
  sapi_header_struct* sapi_header = (sapi_header_struct*)data;
  nr_zend_llist_search_t* search = (nr_zend_llist_search_t*)arg;
  const char* start;

  NR_UNUSED_TSRMLS;

  if (NULL == search) {
    return;
  }

  if (NULL == sapi_header) {
    return;
  }

  if (NULL == sapi_header->header) {
    return;
  }

  if (search->value) {
    return;
  }

  if (sapi_header->header_len <= (unsigned int)search->namelen) {
    return;
  }

  if (0 != nr_strnicmp(sapi_header->header, search->name, search->namelen)) {
    return;
  }

  /*
   * Any amount of leading white space may precede the field value
   * http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.2
   */
  start = sapi_header->header + search->namelen;
  for (; 0 != *start; ++start) {
    if (0 == nr_isspace(*start)) {
      break;
    }
  }

  search->value = nr_strdup(start);
}

static char* nr_php_get_response_header(const char* name TSRMLS_DC) {
  nr_zend_llist_search_t search;

  if (NULL == name) {
    return NULL;
  }

  search.name = name;
  search.namelen = nr_strlen(name);
  search.value = NULL;

  zend_llist_apply_with_argument(nr_php_response_headers(TSRMLS_C),
                                 nr_php_get_response_header_search,
                                 (void*)&search TSRMLS_CC);

  return search.value;
}

int nr_php_has_response_content_length(TSRMLS_D) {
  char* str = nr_php_get_response_header("content-length:" TSRMLS_CC);

  if (NULL == str) {
    return 0;
  }
  nr_free(str);
  return 1;
}

int nr_php_get_response_content_length(TSRMLS_D) {
  int content_length;
  char* str = nr_php_get_response_header("content-length:" TSRMLS_CC);

  if (NULL == str) {
    /* Default should be -1 if content-length header is not present. */
    return -1;
  }

  content_length = (int)strtol(str, 0, 10);
  nr_free(str);

  if (0 == content_length) {
    return -1;
  }
  return content_length;
}

static int has_prefix(const char* s,
                      size_t slen,
                      const char* prefix,
                      size_t prefix_len) {
  return (slen >= prefix_len) && (0 == nr_strnicmp(s, prefix, prefix_len));
}

char* nr_php_get_response_content_type(TSRMLS_D) {
  char* default_content_type;
  const sapi_headers_struct* sapi_headers;
  sapi_header_struct* hdr;
  zend_llist* headers;
  zend_llist_position pos;

  headers = nr_php_response_headers(TSRMLS_C);

  /*
   * If a content-type header has been set, use it because it is what will be
   * sent to the client. If you're wondering why we can't just use
   * SG(sapi_headers).mimetype.
   *
   * This is a manual loop so we can exit early. The headers in this list
   * include the name and value in a single string, and the Content-Type header
   * may include a character encoding, e.g. 'Content-type: text/html;
   * charset=UTF-8'
   */
  for (hdr = (sapi_header_struct*)zend_llist_get_first_ex(headers, &pos); hdr;
       hdr = (sapi_header_struct*)zend_llist_get_next_ex(headers, &pos)) {
    if (has_prefix(hdr->header, hdr->header_len, NR_PSTR("Content-Type:"))) {
      char* mimetype = nr_header_parse_content_type(hdr->header);

      if (mimetype) {
        return mimetype;
      }

      /*
       * Failed to parse the Content-Type header, fallthrough and check if
       * PHP fared any better.
       */
      break;
    }
  }

  /*
   * Check if another extension or SAPI set the mimetype directly. This header
   * does not include the name, but may include a character encoding, e.g.
   * 'text/html; charset=UTF-8'
   */
  sapi_headers = nr_php_sapi_headers(TSRMLS_C);
  if (sapi_headers->mimetype) {
    char* mimetype = nr_header_parse_content_type(sapi_headers->mimetype);

    if (mimetype) {
      return mimetype;
    }
  }

  /*
   * Check if a default content-type was set via INI setting. This header may
   * include the charset but not the name, e.g. 'text/html; charset=UTF-8'
   */
  default_content_type = sapi_get_default_content_type(TSRMLS_C);
  if (default_content_type) {
    char* mimetype = nr_header_parse_content_type(default_content_type);

    efree(default_content_type);

    if (mimetype) {
      return mimetype;
    }
  }

  /* SAPI_DEFAULT_MIMETYPE is 'text/html' */
  return nr_strdup(SAPI_DEFAULT_MIMETYPE);
}

static nr_status_t nr_php_add_response_header(const char* name,
                                              const char* value TSRMLS_DC) {
  int zend_rv;
  sapi_header_line ctr;
  char* header;
  int header_len;

  if ((NULL == name) || (NULL == value)) {
    return NR_FAILURE;
  }

  header = nr_header_format_name_value(name, value, 0);
  header_len = nr_strlen(header);

  nr_memset(&ctr, 0, sizeof(ctr));

  ctr.line = header;
  ctr.line_len = header_len;

  zend_rv = sapi_header_op(SAPI_HEADER_REPLACE, &ctr TSRMLS_CC);

  nr_free(header);

  if (FAILURE == zend_rv) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

void nr_php_header_output_handler(
    char* output NRUNUSED,
    nr_output_buffer_string_len_t output_len NRUNUSED,
    char** handled_output,
    nr_output_buffer_string_len_t* handled_output_len NRUNUSED,
    int mode TSRMLS_DC) {
  int content_length;
  char* response;

  if (handled_output) {
    *handled_output = NULL;
  }

  if (!nr_php_output_is_start(mode)) {
    return;
  }

  if (SG(headers_sent)) {
    nrl_verbosedebug(NRL_TXN,
                     "CAT: unable to add cross process response header: "
                     "headers already sent");
    return;
  }

  if (!nr_php_output_is_end(mode)) {
    nrl_verbosedebug(
        NRL_TXN,
        "CAT: adding cross process response header before buffer's end");
  }

  content_length = nr_php_get_response_content_length(TSRMLS_C);
  response = nr_header_inbound_response(NRPRG(txn), content_length);

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(NRL_CAT, "CAT: inbound response: %s=" NRP_FMT,
                     X_NEWRELIC_APP_DATA, NRP_CAT(response));
  }

  if (NULL == response) {
    nrl_verbosedebug(NRL_TXN,
                     "CAT: unable to create cross process response header");
  } else {
    nr_status_t rv
        = nr_php_add_response_header(X_NEWRELIC_APP_DATA, response TSRMLS_CC);

    if (NR_SUCCESS != rv) {
      nrl_verbosedebug(NRL_TXN, "CAT: failure adding header: %s: " NRP_FMT,
                       X_NEWRELIC_APP_DATA, NRP_CAT(response));
    }
  }

  nr_free(response);
}

/*
 * Sanity check that the sapi_headers pointer is within the bounds of the
 * sapi_globals_struct memory block. We can calculate the block by adding the
 * size of the struct to the address of the first field in
 * sapi_globals_struct. This will be an underestimate in situations where the
 * header struct has been expanded (e.g. OpenSUSE PHP < 5.6) but unless the
 * struct is wildly reordered, these checks should still catch junk pointers.
 * Situations where the struct has been contracted are highly unlikely.
 */
static int nr_php_sapi_headers_pointer_is_plausible(
    sapi_headers_struct* sapi_headers TSRMLS_DC) {
  uintptr_t start = (uintptr_t)&SG(server_context);

  if ((uintptr_t)sapi_headers < start) {
    return 0;
  }

  if ((uintptr_t)sapi_headers >= start + sizeof(sapi_globals_struct)) {
    return 0;
  }

  return 1;
}

int nr_php_header_handler(sapi_header_struct* sapi_header,
                          sapi_header_op_enum op,
                          sapi_headers_struct* sapi_headers TSRMLS_DC) {
  /*
   * Capture a pointer to SG(sapi_headers) to prevent segfaults accessing
   * response headers or status code via the SAPI globals on OpenSUSE. OpenSUSE
   * PHP SAPI globals have a different memory layout.
   */
  if (nr_php_sapi_headers_pointer_is_plausible(sapi_headers TSRMLS_CC)) {
    NRPRG(sapi_headers) = sapi_headers;
  }

  if (NR_PHP_PROCESS_GLOBALS(orig_header_handler)) {
    return NR_PHP_PROCESS_GLOBALS(orig_header_handler)(sapi_header, op,
                                                       sapi_headers TSRMLS_CC);
  }

  /*
   * According to the PHP source code, SAPI_HEADER_ADD should be returned if you
   * don't want the handler to have any effect. It's also what xdebug returns in
   * their header handler.
   */
  return SAPI_HEADER_ADD;
}

sapi_headers_struct* nr_php_sapi_headers(TSRMLS_D) {
  if (NRPRG(sapi_headers)) {
    return NRPRG(sapi_headers);
  }
  return &SG(sapi_headers);
}

zend_llist* nr_php_response_headers(TSRMLS_D) {
  sapi_headers_struct* headers = nr_php_sapi_headers(TSRMLS_C);

  return &headers->headers;
}

void nr_php_capture_sapi_headers(TSRMLS_D) {
  sapi_header_line ctr = {
      .line = NR_NONEXISTENT_HEADER,
      .line_len = sizeof(NR_NONEXISTENT_HEADER) - 1,
      .response_code = 0,
  };

  /*
   * We delete a non-existent header to trigger our own header handler and
   * therefore gain a pointer to the SAPI header globals.
   *
   * We do not use REPLACE for all PHPs because it is riskier than simply
   * deleting a non-existent header.
   */
  sapi_header_op(SAPI_HEADER_DELETE, &ctr TSRMLS_CC);
}

int nr_php_http_response_code(TSRMLS_D) {
  sapi_headers_struct* headers;

  headers = nr_php_sapi_headers(TSRMLS_C);
  return (int)headers->http_response_code;
}
