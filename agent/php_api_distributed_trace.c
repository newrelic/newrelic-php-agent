/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_api.h"
#include "php_api_distributed_trace.h"
#include "php_call.h"
#include "php_hash.h"
#include "nr_header.h"
#include "util_base64.h"
#include "util_hashmap.h"
#include "util_logging.h"
#include "nr_distributed_trace.h"

/* {{{ newrelic\DistributedTracePayload class definition and methods */

/*
 * True global for the DistributedTracePayload class entry.
 */
zend_class_entry* nr_distributed_trace_payload_ce;

static const char payload_text_prop[] = "text";

static const char DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD[]
    = "Function newrelic_accept_distributed_trace_payload() is "
      "deprecated.  Please see "
      "https://docs.newrelic.com/docs/agents/php-agent/features/"
      "distributed-tracing-php-agent#manual "
      "for more details.";

static const char DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD_HTTPSAFE[]
    = "Function newrelic_accept_distributed_trace_payload_httpsafe() is "
      "deprecated.  Please see "
      "https://docs.newrelic.com/docs/agents/php-agent/features/"
      "distributed-tracing-php-agent#manual "
      "for more details.";

static const char DEPRECATION_CREATE_DISTRIBUTED_TRACE_PAYLOAD[]
    = "Function newrelic_create_distributed_trace_payload() is "
      "deprecated.  Please see "
      "https://docs.newrelic.com/docs/agents/php-agent/features/"
      "distributed-tracing-php-agent#manual "
      "for more details.";

/*
 * Arginfo for the DistributedTracePayload methods
 */
ZEND_BEGIN_ARG_INFO_EX(nr_distributed_trace_payload_text_arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_distributed_trace_payload_httpsafe_arginfo_void,
                       0,
                       0,
                       0)
ZEND_END_ARG_INFO()

/*
 * Purpose : Transform a zval array into an axiom hashmap.
 */
static nr_hashmap_t* nr_php_api_distributed_trace_transform_zval_array(
    zval* array) {
  nr_hashmap_t* map = NULL;
  zval* element = NULL;
  nr_php_string_hash_key_t* string_key;
  zend_ulong num_key = 0;

  (void)num_key;

  if (0 == nr_php_is_zval_valid_array(array)) {
    nrl_warning(NRL_API, "Invalid array, expected a string");
    return NULL;
  }

  map = nr_hashmap_create((nr_hashmap_dtor_func_t)nr_hashmap_dtor_str);

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(array), num_key, string_key, element) {
    char* key = NULL;
    char* element_str = NULL;

    if (NULL == element || !nr_php_is_zval_valid_string(element)) {
      nrl_warning(NRL_API, "Invalid array value, expected a string");
      continue;
    }

    if (string_key) {
      /*
       * Was HTTP_ prepended to the beginning of the string? If so, remove it.
       */
      if (0 == nr_strnicmp("http_", ZEND_STRING_VALUE(string_key), 5)) {
        key = nr_string_to_lowercase(ZEND_STRING_VALUE(string_key) + 5);
      } else {
        key = nr_string_to_lowercase(ZEND_STRING_VALUE(string_key));
      }
    }
    element_str = nr_strndup(Z_STRVAL_P(element), Z_STRLEN_P(element));
    nr_hashmap_set(map, key, nr_strlen(key), element_str);
    nr_free(key);
  }
  ZEND_HASH_FOREACH_END();

  return map;
}

/*
 * New Relic API: Accept distributed trace payloads and handle them off to axiom
 *                for further processing.
 *
 * Params : 1. A PHP array of headers.
 *          2. An optional string allowing the user override the
 *             transport type
 *
 * Returns : Boolean for success or failure
 */
#ifdef TAGS
void zif_newrelic_accept_distributed_trace_headers(
    void); /* ctags landing pad only */
void newrelic_accept_distributed_trace_headers(
    void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_accept_distributed_trace_headers) {
  zval* header_array = NULL;
  char* transport_type_arg = NULL;
  char* transport_type_string = NULL;
  nr_hashmap_t* header_map = NULL;

  nr_string_len_t transport_type_arg_length = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nr_php_api_add_supportability_metric(
      "accept_distributed_trace_headers" TSRMLS_CC);

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  if (SUCCESS
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|s", &header_array,
                               &transport_type_arg,
                               &transport_type_arg_length)) {
    if (transport_type_arg) {
      transport_type_string = (char*)nr_alloca(transport_type_arg_length + 1);
      nr_strxcpy(transport_type_string, transport_type_arg,
                 transport_type_arg_length);
    }
    header_map
        = nr_php_api_distributed_trace_transform_zval_array(header_array);
  }

  /* The args are parsed, do the work, and then cleanup. */
  if (nr_php_api_accept_distributed_trace_payload_httpsafe(
          NRPRG(txn), header_map, transport_type_string)) {
    nr_hashmap_destroy(&header_map);
    RETURN_TRUE;
  }

  nr_hashmap_destroy(&header_map);
  RETURN_FALSE;
}

/*
 * DistributedTracePayload methods
 */
static PHP_NAMED_FUNCTION(nr_distributed_trace_payload_httpsafe) {
  char* encoded = NULL;
  int encoded_len = 0;
  zval* text = NULL;
  zval* this_obj = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  this_obj = NR_PHP_INTERNAL_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  text = nr_php_get_zval_object_property(this_obj, payload_text_prop TSRMLS_CC);

  /*
   * nr_b64_encode() will return false if given an empty string, so we'll
   * return here in that case.
   */
  if (0 == Z_STRLEN_P(text)) {
    nr_php_zval_str(return_value, "");
    return;
  }

  encoded = nr_b64_encode(Z_STRVAL_P(text), NRSAFELEN(Z_STRLEN_P(text)),
                          &encoded_len);

  if (NULL == encoded) {
    zend_error(E_WARNING,
               "Error encoding text payload to the HTTP safe format");
    nr_php_zval_str(return_value, "");
    return;
  }

  nr_php_zval_str_len(return_value, encoded, encoded_len);
  nr_free(encoded);
}

static PHP_NAMED_FUNCTION(nr_distributed_trace_payload_text) {
  zval* text = NULL;
  zval* this_obj = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (FAILURE == zend_parse_parameters_none()) {
    RETURN_FALSE;
  }

  this_obj = NR_PHP_INTERNAL_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  text = nr_php_get_zval_object_property(this_obj, payload_text_prop TSRMLS_CC);
  RETURN_ZVAL(text, 1, 0);
}

/*
 * The method array for the DistributedTracePayload class.
 */

// clang-format off
const zend_function_entry nr_distributed_trace_payload_functions[]
    = {ZEND_FENTRY(text,
                   nr_distributed_trace_payload_text,
                   nr_distributed_trace_payload_text_arginfo_void,
                   ZEND_ACC_PUBLIC)
       ZEND_FENTRY(httpSafe,
                   nr_distributed_trace_payload_httpsafe,
                   nr_distributed_trace_payload_httpsafe_arginfo_void,
                   ZEND_ACC_PUBLIC)
       PHP_FE_END};
// clang-format on

/* }}} */

void nr_php_api_distributed_trace_register_userland_class(TSRMLS_D) {
  zend_class_entry tmp_nr_distributed_trace_payload_ce;
  INIT_CLASS_ENTRY(tmp_nr_distributed_trace_payload_ce,
                   "newrelic\\DistributedTracePayload",
                   nr_distributed_trace_payload_functions);

  nr_distributed_trace_payload_ce = zend_register_internal_class(
      &tmp_nr_distributed_trace_payload_ce TSRMLS_CC);

  /*
   * We'll use a true property to store the text to avoid having to abstract
   * the significant differences in how object stores work between PHP 5
   * and 7.
   */
  zend_declare_property_string(
      nr_distributed_trace_payload_ce, nr_remove_const(payload_text_prop),
      sizeof(payload_text_prop) - 1, "", ZEND_ACC_PRIVATE TSRMLS_CC);
}

/*
 * New Relic API: Create a payload for instrumenting an outbound request with
 * Distributed Trace support.
 *    newrelic_create_distributed_trace_payload()
 */
#ifdef TAGS
void zif_newrelic_create_distributed_trace_payload(
    void); /* ctags landing pad only */
void newrelic_create_distributed_trace_payload(
    void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_create_distributed_trace_payload) {
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nrl_warning(NRL_API, DEPRECATION_CREATE_DISTRIBUTED_TRACE_PAYLOAD);
  zend_error(E_DEPRECATED, DEPRECATION_CREATE_DISTRIBUTED_TRACE_PAYLOAD);

  nr_php_api_add_supportability_metric(
      "create_distributed_trace_payload" TSRMLS_CC);

  if (FAILURE == zend_parse_parameters_none()) {
    nrl_warning(
        NRL_API,
        "Unable to parse parameters to "
        "newrelic_create_distributed_trace_payload; %d parameters received",
        ZEND_NUM_ARGS());
    RETURN_FALSE;
  }

  /*
   * With the exception of parameter parsing errors (handled above), we're
   * always going to return a valid object so that the user can
   * unconditionally invoke methods on it.
   */
  object_init_ex(return_value, nr_distributed_trace_payload_ce);

  /* Now we check if we're recording a transaction. */
  if (nr_php_recording(TSRMLS_C)) {
    /*
     * nr_txn_create_distributed_trace_payload() will return NULL if
     * distributed tracing is not enabled, so we don't need to handle that
     * explicitly here.
     */
    char* payload = nr_txn_create_distributed_trace_payload(
        NRPRG(txn), nr_txn_get_current_segment(NRPRG(txn), NULL));

    if (payload) {
      zend_update_property_string(
          nr_distributed_trace_payload_ce, ZVAL_OR_ZEND_OBJECT(return_value),
          nr_remove_const(payload_text_prop), sizeof(payload_text_prop) - 1,
          payload TSRMLS_CC);
      nr_free(payload);
    }
  }
}

/*
 * New Relic API: Add W3c Trace Context and New Relic Distributed Tracing
 *                headers to an existing array of headers
 *
 * Params       : 1. An array of headers or empty array
 *
 * Returns:       True if any headers were successfully inserted into the
 *                provided array, otherwise returns False
 */
#ifdef TAGS
void zif_newrelic_insert_distributed_trace_headers(
    void); /* ctags landing pad only */
void newrelic_insert_distributed_trace_headers(
    void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_insert_distributed_trace_headers) {
  zval* header_array = NULL;
  char* newrelic = NULL;
  char* newrelic_encoded = NULL;
  char* traceparent = NULL;
  char* tracestate = NULL;
  bool any_header_added = false;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nr_php_api_add_supportability_metric(
      "insert_distributed_trace_headers" TSRMLS_CC);

  /* Attempt to parse args for a single array and exit if unable to. */
  if (FAILURE
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a", &header_array)) {
    nrl_warning(NRL_API,
                "Unable to parse parameters to "
                "newrelic_insert_distributed_trace_headers: expected one array "
                "argument.");
    RETURN_FALSE;
  }

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  /*
   * Attempt to create desired headers.
   * Note:
   * newrelic.distributed_tracing_exclude_newrelic_header ini option handling
   * takes place in nr_txn_create_distributed_trace_payload.
   */
  newrelic = nr_txn_create_distributed_trace_payload(
      NRPRG(txn), nr_txn_get_current_segment(NRPRG(txn), NULL));
  traceparent = nr_txn_create_w3c_traceparent_header(
      NRPRG(txn), nr_txn_get_current_segment(NRPRG(txn), NULL));
  tracestate = nr_txn_create_w3c_tracestate_header(
      NRPRG(txn), nr_txn_get_current_segment(NRPRG(txn), NULL));

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
  SEPARATE_ARRAY(header_array);
#endif /* PHP7 */

  /*
   * If a given header was created, insert it into the passed in array.
   * Note: only the newrelic header needs to be base64 encoded .
   */
  if (newrelic) {
    newrelic_encoded = nr_b64_encode(newrelic, nr_strlen(newrelic), 0);
    if (newrelic_encoded) {
      nr_php_add_assoc_string(header_array, NEWRELIC, newrelic_encoded);
      nr_free(newrelic_encoded);
      any_header_added = true;
    }
    nr_free(newrelic);
  }
  if (traceparent) {
    nr_php_add_assoc_string(header_array, W3C_TRACEPARENT, traceparent);
    nr_free(traceparent);
    any_header_added = true;
  }
  if (tracestate) {
    nr_php_add_assoc_string(header_array, W3C_TRACESTATE, tracestate);
    nr_free(tracestate);
    any_header_added = true;
  }

  if (any_header_added) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

bool nr_php_api_accept_distributed_trace_payload(nrtxn_t* txn,
                                                 nr_hashmap_t* header_map,
                                                 char* transport_type) {
  if (NULL == txn) {
    return false;
  }

  return nr_txn_accept_distributed_trace_payload(txn, header_map,
                                                 transport_type);
}

bool nr_php_api_accept_distributed_trace_payload_httpsafe(
    nrtxn_t* txn,
    nr_hashmap_t* header_map,
    char* transport_type) {
  if (NULL == txn) {
    return false;
  }

  return nr_txn_accept_distributed_trace_payload_httpsafe(txn, header_map,
                                                          transport_type);
}

/*
 * New Relic API: Accept A Distributed Trace Payload as a JSON encoded string
 *                and hands it off to axiom for further processing
 *
 * Params       : 1. A string containing an HTTPSafe (Base64)
 *                   JSON encoded payload
 *                2. An optional string allowing the user override the
 *                   transport type
 *
 * Returns:     Boolean for success or failure
 */
#ifdef TAGS
void zif_newrelic_accept_distributed_trace_payload_httpsafe(
    void); /* ctags landing pad only */
void newrelic_accept_distributed_trace_payload_httpsafe(
    void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_accept_distributed_trace_payload_httpsafe) {
  char* payload_arg = NULL;
  char* payload_string = NULL;
  char* type_arg = NULL;
  char* type_string = NULL;
  nr_hashmap_t* header_map = NULL;

  nr_string_len_t payload_arg_length = 0;
  nr_string_len_t type_arg_length = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nrl_warning(NRL_API, DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD_HTTPSAFE);
  zend_error(E_DEPRECATED,
             DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD_HTTPSAFE);

  nr_php_api_add_supportability_metric(
      "accept_distributed_trace_payload" TSRMLS_CC);

  header_map = nr_hashmap_create(NULL);

  if (SUCCESS
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &payload_arg,
                               &payload_arg_length, &type_arg,
                               &type_arg_length)) {
    payload_string = (char*)nr_alloca(payload_arg_length + 1);
    nr_strxcpy(payload_string, payload_arg, payload_arg_length);
    nr_hashmap_set(header_map, NR_PSTR(NEWRELIC), payload_string);

    if (type_arg) {
      type_string = (char*)nr_alloca(type_arg_length + 1);
      nr_strxcpy(type_string, type_arg, type_arg_length);
    }
  }

  /* The args are parsed, do the work, and then cleanup. */
  if (nr_php_api_accept_distributed_trace_payload_httpsafe(
          NRPRG(txn), header_map, type_string)) {
    nr_hashmap_destroy(&header_map);
    RETURN_TRUE;
  }

  nr_hashmap_destroy(&header_map);
  RETURN_FALSE;
}

/*
 * New Relic API: Accepts A Distributed Trace Payload as a JSON encoded string
 *                and hands it off to axiom for further processing
 *
 * Params       : 1. A string containing a JSON encoded payload
 *                2. An optional string allowing the user override the
 *                   transport type
 *
 * Returns:     Boolean for success or failure
 */
#ifdef TAGS
void zif_newrelic_accept_distributed_trace_payload(
    void); /* ctags landing pad only */
void newrelic_accept_distributed_trace_payload(
    void); /* ctags landing pad only */
#endif
PHP_FUNCTION(newrelic_accept_distributed_trace_payload) {
  char* payload_arg = NULL;
  char* payload_string = NULL;
  char* type_arg = NULL;
  char* type_string = NULL;
  nr_hashmap_t* header_map = NULL;

  nr_string_len_t payload_arg_length = 0;
  nr_string_len_t type_arg_length = 0;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  nrl_warning(NRL_API, DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD);
  zend_error(E_DEPRECATED, DEPRECATION_ACCEPT_DISTRIBUTED_TRACE_PAYLOAD);

  nr_php_api_add_supportability_metric(
      "accept_distributed_trace_payload" TSRMLS_CC);

  header_map = nr_hashmap_create(NULL);

  if (SUCCESS
      == zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|s", &payload_arg,
                               &payload_arg_length, &type_arg,
                               &type_arg_length)) {
    payload_string = (char*)nr_alloca(payload_arg_length + 1);
    nr_strxcpy(payload_string, payload_arg, payload_arg_length);
    nr_hashmap_set(header_map, NR_PSTR(NEWRELIC), payload_string);

    if (type_arg) {
      type_string = (char*)nr_alloca(type_arg_length + 1);
      nr_strxcpy(type_string, type_arg, type_arg_length);
    }
  }

  if (!nr_php_recording(TSRMLS_C)) {
    RETURN_FALSE;
  }

  /* The args are parsed, now do the work. */
  if (nr_php_api_accept_distributed_trace_payload(NRPRG(txn), header_map,
                                                  type_string)) {
    nr_hashmap_destroy(&header_map);
    RETURN_TRUE;
  }

  nr_hashmap_destroy(&header_map);
  RETURN_FALSE;
}
