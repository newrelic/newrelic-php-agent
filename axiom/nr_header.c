/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "nr_header.h"
#include "nr_header_private.h"
#include "nr_txn.h"
#include "util_base64.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_obfuscate.h"
#include "util_object.h"
#include "util_strings.h"

/*
 * Purpose : Determine if the given string contains only base 64 characters.
 */
nr_status_t nr_header_validate_encoded_string(const char* encoded_string) {
  int i;

  if ((0 == encoded_string) || (0 == encoded_string[0])) {
    return NR_FAILURE;
  }

  for (i = 0; encoded_string[i]; i++) {
    if (0 == nr_b64_is_valid_character(encoded_string[i])) {
      return NR_FAILURE;
    }
  }

  return NR_SUCCESS;
}

nr_hashmap_t* nr_header_create_distributed_trace_map(const char* nr_header,
                                                     const char* traceparent,
                                                     const char* tracestate) {
  nr_hashmap_t* header_map = NULL;

  if (NULL == nr_header && NULL == traceparent) {
    return NULL;
  }

  header_map = nr_hashmap_create(NULL);
  if (nr_header) {
    nr_hashmap_set(header_map, NR_PSTR(NEWRELIC), nr_strdup(nr_header));
  }
  if (traceparent) {
    nr_hashmap_set(header_map, NR_PSTR(W3C_TRACEPARENT),
                   nr_strdup(traceparent));
  }
  if (tracestate) {
    nr_hashmap_set(header_map, NR_PSTR(W3C_TRACESTATE), nr_strdup(tracestate));
  }

  return header_map;
}

char* nr_header_encode(const nrtxn_t* txn, const char* string) {
  const char* encoding_key;

  if (0 == txn) {
    return 0;
  }

  encoding_key = nro_get_hash_string(txn->app_connect_reply, "encoding_key", 0);
  return nr_obfuscate(string, encoding_key, 0);
}

char* nr_header_decode(const nrtxn_t* txn, const char* encoded_string) {
  const char* encoding_key;

  if (0 == txn) {
    return 0;
  }
  if (NR_SUCCESS != nr_header_validate_encoded_string(encoded_string)) {
    return 0;
  }

  encoding_key = nro_get_hash_string(txn->app_connect_reply, "encoding_key", 0);
  return nr_deobfuscate(encoded_string, encoding_key, 0);
}

int64_t nr_header_account_id_from_cross_process_id(
    const char* cross_process_id) {
  int64_t account_id;
  char* end = NULL;

  if (NULL == cross_process_id) {
    return -1;
  }

  /*
   * The cross_process_id should have the following format: 12345#6789
   * where 12345 is the account_id of interest. Therefore the strtoll should
   * stop at a '#' character.
   */
  end = 0;
  account_id = (int64_t)strtoll(cross_process_id, &end, 10);

  if ((0 == end) || ('#' != *end)) {
    return -1;
  }
  if (account_id >= INT_MAX) {
    return -1;
  }

  return account_id;
}

nr_status_t nr_header_validate_decoded_id(const nrtxn_t* txn,
                                          const char* decoded_id) {
  int64_t account_id;
  int len;

  if ((0 == txn) || (0 == decoded_id)) {
    return NR_FAILURE;
  }

  len = nr_strlen(decoded_id);
  if (len >= NR_CROSS_PROCESS_ID_LENGTH_MAX) {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: cross process id is invalid");
    }
    return NR_FAILURE;
  }

  account_id = nr_header_account_id_from_cross_process_id(decoded_id);
  if (-1 == account_id) {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: account id is missing or invalid");
    }
    return NR_FAILURE;
  }

  if (nr_txn_is_account_trusted(txn, (int)account_id)) {
    return NR_SUCCESS;
  } else {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: account is untrusted: id=%d",
                       (int)account_id);
    }
    return NR_FAILURE;
  }
}

char* nr_header_inbound_response_internal(nrtxn_t* txn, int content_length) {
  nrtime_t qtime;
  char buf[32 + NR_CROSS_PROCESS_ID_LENGTH_MAX];
  const char* guid;
  nrobj_t* obj;
  char* json;
  double qtime_seconds;
  double apptime_seconds;
  const char* cross_process_id;
  nrtime_t apptime;

  if ((0 == txn) || (0 == txn->status.recording)
      || (0 == txn->options.cross_process_enabled)) {
    return 0;
  }
  if (0 == txn->cat.client_cross_process_id) {
    return 0;
  }

  apptime = nr_txn_unfinished_duration(txn);

  cross_process_id
      = nro_get_hash_string(txn->app_connect_reply, "cross_process_id", 0);
  if (0 == cross_process_id) {
    return 0;
  }
  guid = nr_txn_get_guid(txn);
  if (NULL == guid) {
    return 0;
  }

  if (NR_STATUS_CROSS_PROCESS_START != txn->status.cross_process) {
    /*
     * The status.cross_process field prevents this function from being
     * called multiple times.  Multiple calls would create inaccurate
     * ClientApplication metrics.
     */
    return 0;
  }

  if (NR_SUCCESS != nr_txn_freeze_name_update_apdex(txn)) {
    return 0;
  }

  qtime = nr_txn_queue_time(txn);

  nro_set_hash_string(txn->intrinsics, "client_cross_process_id",
                      txn->cat.client_cross_process_id);
  snprintf(buf, sizeof(buf), "ClientApplication/%s/all",
           txn->cat.client_cross_process_id);
  nrm_add(txn->unscoped_metrics, buf, apptime);
  qtime_seconds = ((double)qtime) / NR_TIME_DIVISOR_D;
  apptime_seconds = ((double)apptime) / NR_TIME_DIVISOR_D;

  obj = nro_new_array();
  nro_set_array_string(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_CROSS_PROCESS_ID,
                       cross_process_id);
  nro_set_array_string(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_TXN_NAME,
                       txn->name);
  nro_set_array_double(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_QUEUE_TIME,
                       qtime_seconds);
  nro_set_array_double(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_RESPONSE_TIME,
                       apptime_seconds);
  nro_set_array_int(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_CONTENT_LENGTH,
                    content_length);
  nro_set_array_string(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_GUID, guid);
  nro_set_array_boolean(obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_RECORD_TT,
                        0); /* record_tt currently always false */

  json = nro_to_json(obj);
  nro_delete(obj);

  txn->status.cross_process = NR_STATUS_CROSS_PROCESS_RESPONSE_CREATED;

  return json;
}

char* nr_header_inbound_synthetics(const nrtxn_t* txn,
                                   const char* x_newrelic_synthetics) {
  return nr_header_decode(txn, x_newrelic_synthetics);
}

char* nr_header_inbound_response(nrtxn_t* txn, int content_length) {
  char* response;
  char* encoded_response;

  response = nr_header_inbound_response_internal(txn, content_length);
  encoded_response = nr_header_encode(txn, response);

  nr_free(response);

  return encoded_response;
}

char* nr_header_outbound_request_synthetics_encoded(const nrtxn_t* txn) {
  const char* decoded;

  if (NULL == txn) {
    return NULL;
  }

  if (0 == txn->options.synthetics_enabled) {
    return NULL;
  }

  decoded = nr_synthetics_outbound_header(txn->synthetics);
  return nr_header_encode(txn, decoded);
}

void nr_header_outbound_request_decoded(nrtxn_t* txn,
                                        char** decoded_id_ptr,
                                        char** decoded_transaction_ptr) {
  nrobj_t* array;
  const char* cross_process_id;
  const char* guid;
  char* path_hash;
  const char* trip_id;

  if (0 == txn) {
    return;
  }

  /*
   * Bail here if CAT is disabled so as not to generate X-NewRelic-Id and
   * X-NewRelic-Transaction headers.
   */
  if (0 == txn->options.cross_process_enabled) {
    return;
  }

  cross_process_id
      = nro_get_hash_string(txn->app_connect_reply, "cross_process_id", 0);
  if (0 == cross_process_id) {
    return;
  }
  guid = nr_txn_get_guid(txn);
  if (NULL == guid) {
    return;
  }

  /* x-newrelic-id header */
  *decoded_id_ptr = nr_strdup(cross_process_id);

  /* x-newrelic-transaction */
  trip_id = nr_txn_get_cat_trip_id(txn);
  path_hash = nr_txn_get_path_hash(txn);

  array = nro_new_array();
  nro_set_array_string(array, 1, guid);
  nro_set_array_boolean(array, 2, 0); /* record_tt currently always false */
  nro_set_array_string(array, 3, trip_id);
  nro_set_array_string(array, 4, path_hash);
  *decoded_transaction_ptr = nro_to_json(array);
  nro_delete(array);
  nr_free(path_hash);

  txn->type |= NR_TXN_TYPE_CAT_OUTBOUND;
}

static void nr_header_outbound_save(nr_hashmap_t* outbound_headers,
                                    const char* key,
                                    char* header) {
  if (NULL == header) {
    return;
  }

  nr_hashmap_update(outbound_headers, key, nr_strlen(key), header);
}

nr_hashmap_t* nr_header_outbound_request_create(nrtxn_t* txn,
                                                nr_segment_t* segment) {
  char* decoded_id = NULL;
  char* decoded_transaction = NULL;
  char* x_newrelic_id_ptr = NULL;
  char* x_newrelic_transaction_ptr = NULL;
  char* x_newrelic_synthetics_ptr = NULL;
  char* newrelic_ptr = NULL;
  char* newrelic_payload = NULL;
  const char* tracing_vendors = NULL;
  char* traceparent_ptr = NULL;
  char* tracestate_ptr = NULL;
  nr_hashmap_t* outbound_headers = NULL;

  if (NULL == txn || NULL == segment) {
    return NULL;
  }

  outbound_headers
      = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_hashmap_dtor_str);

  if (txn->options.distributed_tracing_enabled) {
    if (!txn->options.distributed_tracing_exclude_newrelic_header) {
      newrelic_payload = nr_txn_create_distributed_trace_payload(txn, segment);
      if (newrelic_payload) {
        newrelic_ptr
            = nr_b64_encode(newrelic_payload, nr_strlen(newrelic_payload), 0);
        nr_header_outbound_save(outbound_headers, NEWRELIC, newrelic_ptr);
        nr_free(newrelic_payload);
      }
    }

    traceparent_ptr = nr_txn_create_w3c_traceparent_header(txn, segment);
    nr_header_outbound_save(outbound_headers, W3C_TRACEPARENT, traceparent_ptr);

    tracestate_ptr = nr_txn_create_w3c_tracestate_header(txn, segment);
    tracing_vendors = nr_distributed_trace_inbound_get_raw_tracing_vendors(
        txn->distributed_trace);

    if (tracing_vendors && tracestate_ptr) {
      tracestate_ptr = nr_str_append(tracestate_ptr, tracing_vendors);
    }
    nr_header_outbound_save(outbound_headers, W3C_TRACESTATE, tracestate_ptr);

    txn->type |= NR_TXN_TYPE_DT_OUTBOUND;

  } else if (txn->options.cross_process_enabled) {
    nr_header_outbound_request_decoded(txn, &decoded_id, &decoded_transaction);

    x_newrelic_id_ptr = nr_header_encode(txn, decoded_id);
    x_newrelic_transaction_ptr = nr_header_encode(txn, decoded_transaction);

    nr_header_outbound_save(outbound_headers, X_NEWRELIC_ID, x_newrelic_id_ptr);
    nr_header_outbound_save(outbound_headers, X_NEWRELIC_TRANSACTION,
                            x_newrelic_transaction_ptr);
  }

  /*
   * The synthetics header should always be sent, regardless of whether CAT is
   * enabled.
   *
   * However, this can be disabled altogether with the
   * newrelic.synthetics.enabled setting.
   */
  x_newrelic_synthetics_ptr
      = nr_header_outbound_request_synthetics_encoded(txn);
  nr_header_outbound_save(outbound_headers, X_NEWRELIC_SYNTHETICS,
                          x_newrelic_synthetics_ptr);

  nr_free(decoded_id);
  nr_free(decoded_transaction);

  return outbound_headers;
}

static void nr_header_outbound_response_object(nrtxn_t* txn,
                                               const nrobj_t* response_obj,
                                               char** external_id_ptr,
                                               char** external_txnname_ptr,
                                               char** external_guid_ptr) {
  nr_status_t rv;
  int response_size;
  const char* external_id = 0;
  const char* external_txnname = 0;
  const char* external_guid = 0;

  if ((0 == txn) || (0 == response_obj)
      || (0 == txn->options.cross_process_enabled)) {
    return;
  }

  if (NR_OBJECT_ARRAY != nro_type(response_obj)) {
    return;
  }

  response_size = nro_getsize(response_obj);
  if (response_size < NR_RESPONSE_HDR_MIN_FIELDS) {
    return;
  }

  external_id = nro_get_array_string(
      response_obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_CROSS_PROCESS_ID, 0);
  if (0 == external_id) {
    return;
  }

  rv = nr_header_validate_decoded_id(txn, external_id);
  if (NR_SUCCESS != rv) {
    return;
  }

  external_txnname = nro_get_array_string(
      response_obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_TXN_NAME, 0);
  if (0 == external_txnname) {
    return;
  }

  if (response_size >= (int)NR_RESPONSE_HDR_FIELD_INDEX_GUID) {
    external_guid = nro_get_array_string(
        response_obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_GUID, 0);
    if (0 == external_guid) {
      return;
    }
  }

  if (response_size >= (int)NR_RESPONSE_HDR_FIELD_INDEX_RECORD_TT) {
    nr_status_t err = NR_FAILURE;
    int external_record_tt = 0;

    external_record_tt = nro_get_array_boolean(
        response_obj, (int)NR_RESPONSE_HDR_FIELD_INDEX_RECORD_TT, &err);
    if (NR_SUCCESS != err) {
      return;
    }

    if (external_record_tt) {
      txn->status.has_outbound_record_tt = 1;
    }
  }

  if (external_id_ptr) {
    *external_id_ptr = nr_strdup(external_id);
  }
  if (external_txnname_ptr) {
    *external_txnname_ptr = nr_strdup(external_txnname);
  }
  if (external_guid_ptr && external_guid) {
    *external_guid_ptr = nr_strdup(external_guid);
  }
}

void nr_header_outbound_response_decoded(nrtxn_t* txn,
                                         const char* decoded_response,
                                         char** external_id_ptr,
                                         char** external_txnname_ptr,
                                         char** external_guid_ptr) {
  nrobj_t* response_obj = nro_create_from_json(decoded_response);

  nr_header_outbound_response_object(txn, response_obj, external_id_ptr,
                                     external_txnname_ptr, external_guid_ptr);

  nro_delete(response_obj);
}

void nr_header_outbound_response(nrtxn_t* txn,
                                 const char* x_newrelic_app_data,
                                 char** external_id_ptr,
                                 char** external_txnname_ptr,
                                 char** external_guid_ptr) {
  char* decoded_response = nr_header_decode(txn, x_newrelic_app_data);

  if (external_id_ptr) {
    *external_id_ptr = 0;
  }
  if (external_txnname_ptr) {
    *external_txnname_ptr = 0;
  }
  if (external_guid_ptr) {
    *external_guid_ptr = 0;
  }

  nr_header_outbound_response_decoded(txn, decoded_response, external_id_ptr,
                                      external_txnname_ptr, external_guid_ptr);
  nr_free(decoded_response);
}

char* nr_header_extract_encoded_value(const char* header_name,
                                      const char* string) {
  int name_start;
  int value_start;
  int value_end;

  if ((0 == string) || (0 == header_name)) {
    return 0;
  }

  name_start = nr_strcaseidx(string, header_name);
  if (name_start < 0) {
    return 0;
  }

  value_start = name_start + nr_strlen(header_name);

  /* Skip over the colon and any spaces between the header name its value. */
  while ((':' == string[value_start]) || (' ' == string[value_start])) {
    value_start += 1;
  }

  value_end = value_start;
  while (nr_b64_is_valid_character(string[value_end])) {
    value_end += 1;
  }

  if (value_end <= value_start) {
    return 0;
  }

  return nr_strndup(string + value_start, value_end - value_start);
}

char* nr_header_format_name_value(const char* name,
                                  const char* value,
                                  int include_return_newline) {
  int len;
  char* hdr;
  const char* suffix = include_return_newline ? "\r\n" : "";

  if ((0 == name) || (0 == value)) {
    return 0;
  }

  len = nr_strlen(name) + nr_strlen(value) + 8;
  hdr = (char*)nr_malloc(len);

  snprintf(hdr, len, "%s: %s%s", name, value, suffix);

  return hdr;
}

/*
 * Check whether a character is valid in an HTTP token.
 * HTTP tokens have the following (ABNF) form.
 *
 *   tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*" / "+" / "-" / "." /
 *           "^" / "_" / "`" / "|" / "~" / DIGIT / ALPHA
 *   token = 1*tchar
 *
 * See: http://tools.ietf.org/html/rfc7230#appendix-B
 */
static int nr_header_is_valid_token_char(char ch) {
  if (('a' <= ch) && (ch <= 'z')) {
    return 1;
  }

  if (('A' <= ch) && (ch <= 'Z')) {
    return 1;
  }

  if (('0' <= ch) && (ch <= '9')) {
    return 1;
  }

  switch (ch) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
      return 1;
    default:
      return 0; /* invalid character */
  }
}

/*
 * Parse an HTTP Content-Type header to extract the mime type. A valid
 * header has the (ABNF) form:
 *
 *   content-type = media-type
 *   media-type   = type "/" subtype *( OWS ";" OWS parameter )
 *   type         = token
 *   subtype      = token
 *   parameter    = token "=" ( token / quoted-string )
 *   OWS          = SPACE | HTAB
 *
 * See: http://tools.ietf.org/html/rfc7231#section-3.1.1.1
 */
char* nr_header_parse_content_type(const char* header) {
  const char* colon;
  const char* start = header;
  int len = 0;

  if (0 == header) {
    return NULL;
  }

  colon = nr_strchr(header, ':');

  if (colon == header) {
    /* header-name is empty */
    return NULL;
  }

  /* skip to the colon if we got a name prefix */
  if (0 != colon) {
    start = colon + 1;
  }

  /* find the start of the media-type */
  for (; 0 != *start; ++start) {
    if ((*start != ' ') && (*start != '\t')) {
      break;
    }
  }

  /*
   * allow media-type to be empty. PHP treats this specially to indicate
   * the content-type header should not be sent to the client.
   */
  if ((0 == *start) && (0 != colon)) {
    return nr_strdup("");
  }

  /* consume the type */
  while (0 != start[len]) {
    if (0 == nr_header_is_valid_token_char(start[len])) {
      break;
    }

    len += 1;
  }

  if (0 == len) {
    /* type was empty */
    return NULL;
  }

  /* type must be followed by slash */
  if ('/' != start[len]) {
    return NULL;
  }

  len += 1;

  /* now consume the subtype */
  while (0 != start[len]) {
    if (0 == nr_header_is_valid_token_char(start[len])) {
      break;
    }

    len += 1;
  }

  /* subtype can only be followed by whitespace or a semicolon */
  switch (start[len]) {
    case 0:
    case ' ':
    case '\t':
    case ';':
      break;
    default:
      return NULL;
  }

  if ('/' == start[len - 1]) {
    /* subtype was empty */
    return NULL;
  }

  /* all that remains are the parameters, which we ignore */

  return nr_strndup(start, len);
}

static int nr_header_obj_is_null_or_string(const nrobj_t* obj) {
  return (NULL == obj) || (NR_OBJECT_NONE == nro_type(obj))
         || (NR_OBJECT_STRING == nro_type(obj));
}

nr_status_t nr_header_process_x_newrelic_transaction(
    nrtxn_t* txn,
    const nrobj_t* x_newrelic_txn) {
  const char* inbound_guid;
  int inbound_record_tt;
  const nrobj_t* referring_path_hash_val;
  const char* referring_path_hash;
  const nrobj_t* trip_id_val;
  const char* trip_id;

  /*
   * The first two fields are mandatory; the second two came in as part of
   * CATv2.
   */
  inbound_guid = nro_get_array_string(x_newrelic_txn, 1, NULL);
  inbound_record_tt = nro_get_array_boolean(x_newrelic_txn, 2, NULL);
  if ((NULL == inbound_guid) || (-1 == inbound_record_tt)) {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: guid or record_tt missing or invalid");
    }
    return NR_FAILURE;
  }

  trip_id_val = nro_get_array_value(x_newrelic_txn, 3, NULL);
  referring_path_hash_val = nro_get_array_value(x_newrelic_txn, 4, NULL);

  /*
   * If trip_id and referring_path_hash exist, they should not be malformed.
   */
  if (0 == nr_header_obj_is_null_or_string(trip_id_val)) {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: trip id is invalid");
    }
    return NR_FAILURE;
  }
  if (0 == nr_header_obj_is_null_or_string(referring_path_hash_val)) {
    if (txn->special_flags.debug_cat) {
      nrl_verbosedebug(NRL_CAT, "CAT: referring path hash is invalid");
    }
    return NR_FAILURE;
  }

  txn->type |= NR_TXN_TYPE_CAT_INBOUND;
  nr_free(txn->cat.inbound_guid);
  txn->cat.inbound_guid = nr_strdup(inbound_guid);
  txn->status.has_inbound_record_tt = inbound_record_tt ? 1 : 0;
  nro_set_hash_string(txn->intrinsics, "referring_transaction_guid",
                      inbound_guid);

  trip_id = nro_get_string(trip_id_val, NULL);
  if (trip_id) {
    nr_free(txn->cat.trip_id);
    txn->cat.trip_id = nr_strdup(trip_id);
  }

  referring_path_hash = nro_get_string(referring_path_hash_val, NULL);
  if (referring_path_hash) {
    nr_free(txn->cat.referring_path_hash);
    txn->cat.referring_path_hash = nr_strdup(referring_path_hash);
  }

  return NR_SUCCESS;
}

nr_status_t nr_header_set_cat_txn(nrtxn_t* txn,
                                  const char* x_newrelic_id,
                                  const char* x_newrelic_transaction) {
  char* decoded_id = NULL;
  char* decoded_txn = NULL;
  nrobj_t* fields = NULL;
  nr_status_t rv = NR_FAILURE;

  if (NULL == txn) {
    return NR_FAILURE;
  }

  if (txn->special_flags.debug_cat) {
    nrl_verbosedebug(NRL_CAT,
                     "CAT: inbound request: %s=" NRP_FMT " %s=" NRP_FMT,
                     X_NEWRELIC_ID, NRP_CAT(x_newrelic_id),
                     X_NEWRELIC_TRANSACTION, NRP_CAT(x_newrelic_transaction));
  }

  decoded_id = nr_header_decode(txn, x_newrelic_id);

  /*
   * Check if the account is trusted.
   */
  if (NR_FAILURE == nr_header_validate_decoded_id(txn, decoded_id)) {
    goto end;
  }

  nr_free(txn->cat.client_cross_process_id);
  txn->cat.client_cross_process_id = nr_strdup(decoded_id);

  decoded_txn = nr_header_decode(txn, x_newrelic_transaction);
  if (NULL == decoded_txn) {
    goto end;
  }

  /*
   * Process the X-NewRelic-Transaction fields into the transaction struct and
   * add the intrinsics we can right now.
   */
  fields = nro_create_from_json(decoded_txn);
  if (NULL == fields) {
    goto end;
  }

  rv = nr_header_process_x_newrelic_transaction(txn, fields);

end:
  nr_free(decoded_id);
  nr_free(decoded_txn);
  nro_delete(fields);

  return rv;
}

nr_status_t nr_header_set_synthetics_txn(nrtxn_t* txn, const char* header) {
  int account = 0;
  char* decoded = NULL;
  nr_synthetics_t* synthetics = NULL;

  if ((NULL == txn) || (NULL == header)) {
    return NR_FAILURE;
  }

  if (NULL != txn->synthetics) {
    nrl_verbosedebug(NRL_TXN, "%s: transaction already has synthetics",
                     __func__);
    return NR_FAILURE;
  }

  /*
   * Decode the given header, and attempt to create a synthetics object.
   * nr_synthetics_create and nr_free will both handle decoded == NULL
   * appropriately, so we don't need to have an explicit check here.
   */
  decoded = nr_header_inbound_synthetics(txn, header);
  synthetics = nr_synthetics_create(decoded);
  nr_free(decoded);

  if (NULL == synthetics) {
    return NR_FAILURE;
  }

  /*
   * Check if the account ID in the synthetics header is trusted.
   */
  account = nr_synthetics_account_id(synthetics);
  if (!nr_txn_is_account_trusted(txn, account)) {
    nrl_verbosedebug(NRL_TXN, "%s: account ID %d is not trusted", __func__,
                     account);

    nr_synthetics_destroy(&synthetics);
    return NR_FAILURE;
  }

  /*
   * We are good. Let's treat this as a synthetics transaction.
   */
  txn->synthetics = synthetics;
  txn->type |= NR_TXN_TYPE_SYNTHETICS;

  return NR_SUCCESS;
}
