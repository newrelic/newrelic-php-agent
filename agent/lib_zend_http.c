/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "lib_zend_http.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "util_logging.h"

typedef enum _nr_zend_http_adapter {
  NR_ZEND_ADAPTER_UNKNOWN = -1,
  NR_ZEND_ADAPTER_CURL = 0,
  NR_ZEND_ADAPTER_OTHER = 1,
} nr_zend_http_adapter;

/*
 * Laminas is a rebranding of Zend, but the logic remains the same,
 * it is simply a name change and corresponds directly to Zend 3.x.
 *
 * Refer to the manual for up to date specs:
 * https://docs.laminas.dev/
 *
 * In order to toggle between `zend` and `laminas` naming schemes, the names
 * are stored in defines.  The default is the `zend` naming scheme.
 * If `laminas` is detected, the values are redefined in this file's
 * `nr_laminas_http_enable` function.
 */
#define LIB_NAME_Z "Zend"
#define CURL_ADAPTER_Z "Zend_Http_Client_Adapter_Curl"
#define URI_HTTP_Z  "Zend_Uri_Http"
#define HTTP_CLIENT_Z  "Zend_Http_Client"
#define HTTP_CLIENT_REQUEST_Z "Zend_Http_Client::request"

#define LIB_NAME_L "Laminas"
#define CURL_ADAPTER_L "Laminas\\Http\\Client\\Adapter\\Curl::class"
#define URI_HTTP_L "Laminas\\Uri\\Http"
#define HTTP_CLIENT_L "Laminas\\Http\\Client"
#define HTTP_CLIENT_REQUEST_L "Laminas\\Http\\Client::send"

char * library_name = LIB_NAME_Z;
char * curl_adapter_typename = CURL_ADAPTER_Z;
char * uri_http_typename = URI_HTTP_Z;
char * http_client = HTTP_CLIENT_Z;
char * http_client_request = HTTP_CLIENT_REQUEST_Z;

/*
 * Purpose : Determine which HTTP client adapter is being used by a Zend
 *           external call.
 *
 * Params  : 1. A zval representing an instance of Zend_Http_Client.
 *
 * Returns : One of the nr_zend_http_adapter enum values.
 */
static nr_zend_http_adapter nr_zend_check_adapter(zval* this_var TSRMLS_DC) {
  zval* config = NULL;
  zval* adapter_ivar = NULL;
  zval* adapter_val = NULL;

  if (NULL == this_var) {
    return NR_ZEND_ADAPTER_UNKNOWN;
  }

  /*
   * How we determine which adapter is being used.
   *   1) check if $adapter has been initialized
   *   2) if yes, check if it is an instance of the cURL adapter
   *   3) otherwise, check if the config hash contains an 'adapter' key
   *   4) if present, check whether its value is an instance of the cURL
   *      adapter or a string representing the cURL adapter's typename.
   */

  adapter_ivar = nr_php_get_zval_object_property(this_var, "adapter" TSRMLS_CC);
  if (nr_php_is_zval_valid_object(adapter_ivar)) {
    if (nr_php_object_instanceof_class(adapter_ivar,
                                       curl_adapter_typename TSRMLS_CC)) {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: adapter is Curl", library_name);
      return NR_ZEND_ADAPTER_CURL;
    }
    return NR_ZEND_ADAPTER_OTHER;
  }

  config = nr_php_get_zval_object_property(this_var, "config" TSRMLS_CC);
  if ((0 == config) || (IS_ARRAY != Z_TYPE_P(config))) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: this->config is not array",
                     library_name);
    return NR_ZEND_ADAPTER_UNKNOWN;
  }

  adapter_val = nr_php_zend_hash_find(Z_ARRVAL_P(config), "adapter");
  if (NULL == adapter_val) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: unable to find adapter in this->config",
                     library_name);
    return NR_ZEND_ADAPTER_UNKNOWN;
  }

  if (nr_php_is_zval_valid_string(adapter_val)) {
    if (0
        == nr_strncaseidx(Z_STRVAL_P(adapter_val), curl_adapter_typename,
                          Z_STRLEN_P(adapter_val))) {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: adapter is Curl", library_name);
      return NR_ZEND_ADAPTER_CURL;
    }
    return NR_ZEND_ADAPTER_OTHER;
  }

  if (nr_php_is_zval_valid_object(adapter_val)) {
    if (nr_php_object_instanceof_class(adapter_val,
                                       curl_adapter_typename TSRMLS_CC)) {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: adapter is Curl", library_name);
      return NR_ZEND_ADAPTER_CURL;
    }
    return NR_ZEND_ADAPTER_OTHER;
  }

  nrl_verbosedebug(NRL_FRAMEWORK,
                   "%s: this->config['adapter'] is not string or object",
                   library_name);
  return NR_ZEND_ADAPTER_UNKNOWN;
}

/*
 * Purpose : For Zend, get the url of a Zend_Http_Client instance before a
 *           Zend_Http_Client::request call.
 *           For Laminas, get the url of a Zend_Http_Client instance before a
 *           Laminas_Http_Client::send call.
 *
 * Params  : 1. For Zend, the instance receiver of the
 *              Zend_Http_Client::request call.
 *              For Laminas, the instance receiver of the
 *              Zend_Http_Client::send call.
 *           2. A pointer to a string that will receive the URL. It is the
 *              responsibility of the caller to free this URL.
 *
 * Returns : NR_SUCCESS or NR_FAILURE. If NR_FAILURE is returned, the url and
 *           url_len pointers will be unchanged.
 */
static nr_status_t nr_zend_http_client_request_get_url(zval* this_var,
                                                       char** url_ptr
                                                           TSRMLS_DC) {
  nr_status_t retval = NR_FAILURE;
  zval* uri = NULL;
  zval* rval = NULL;

  if ((NULL == this_var) || (NULL == url_ptr)) {
    goto end;
  }

  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: this not an object: %d", library_name,
                     Z_TYPE_P(this_var));
    goto end;
  }

  uri = nr_php_get_zval_object_property(this_var, "uri" TSRMLS_CC);
  if (NULL == uri) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: no URI", library_name);
    goto end;
  }

  if (0 == nr_php_object_instanceof_class(uri, uri_http_typename TSRMLS_CC)) {
    if (nr_php_is_zval_valid_object(uri)) {
      nrl_verbosedebug(
          NRL_FRAMEWORK, "%s: URI is wrong class: %*s.", library_name,
          NRSAFELEN(nr_php_class_entry_name_length(Z_OBJCE_P(uri))),
          nr_php_class_entry_name(Z_OBJCE_P(uri)));
    } else {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: URI is not an object: %d",
                       library_name, Z_TYPE_P(uri));
    }
    goto end;
  }

  /*
   * Ok, this_var->uri seems to exist and seems to be of the right type. Now we
   * need to call uri->getUri() to get the name of the url to use.
   */

  rval = nr_php_call(uri, "getUri");
  if (nr_php_is_zval_non_empty_string(rval)) {
    *url_ptr = nr_strndup(Z_STRVAL_P(rval), Z_STRLEN_P(rval));
    retval = NR_SUCCESS;
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: uri->getUri() failed", library_name);
  }

end:
  nr_php_zval_free(&rval);
  return retval;
}

/*
 * Purpose : Add the cross process request headers to a
 *           Zend_Http_Client::request call by using the
 *           Zend_Http_Client::setHeaders method.
 */
static void nr_zend_http_client_request_add_request_headers(
    zval* this_var,
    nr_segment_t* segment TSRMLS_DC) {
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* value = NULL;
  size_t i;
  size_t header_count;
  zval* arr;
  zval* retval;

  if (NULL == this_var) {
    return;
  }
  if (!nr_php_is_zval_valid_object(this_var)) {
    return;
  }

  outbound_headers = nr_header_outbound_request_create(NRPRG(txn), segment);

  if (NULL == outbound_headers) {
    return;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='%s' %s=" NRP_FMT " %s=" NRP_FMT,
        http_client, X_NEWRELIC_ID,
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

  retval = nr_php_call(this_var, "setHeaders", arr);

  nr_php_zval_free(&arr);
  nr_php_zval_free(&retval);
  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}

/*
 * Purpose : Get the cross process response header after a
 *           Zend_Http_Client::request call by using the
 *           Zend_Http_Response::getHeader method.
 */
static char* nr_zend_http_client_request_get_response_header(
    zval* response TSRMLS_DC) {
  zval* header_name = NULL;
  zval* retval = NULL;
  char* response_header = NULL;

  if (NULL == NRPRG(txn)) {
    return NULL;
  }
  if (!NRPRG(txn)->options.cross_process_enabled) {
    return NULL;
  }

  if (!nr_php_is_zval_valid_object(response)) {
    return NULL;
  }

  header_name = nr_php_zval_alloc();
  nr_php_zval_str(header_name, X_NEWRELIC_APP_DATA);

  retval = nr_php_call(response, "getHeader", header_name);

  nr_php_zval_free(&header_name);
  if (1 == nr_php_is_zval_non_empty_string(retval)) {
    response_header = nr_strndup(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
  }

  nr_php_zval_free(&retval);

  return response_header;
}

/*
 * Purpose : Get the response code after a
 *           Zend_Http_Client::request call by using the
 *           Zend_Http_Response::getResponseCode method.
 */
static uint64_t nr_zend_http_client_request_get_response_code(
    zval* response TSRMLS_DC) {
  zval* retval = NULL;
  uint64_t response_code = 0;

  if (NULL == NRPRG(txn)) {
    return 0;
  }

  if (!nr_php_is_zval_valid_object(response)) {
    return 0;
  }

  retval = nr_php_call(response, "getResponseCode");

  if (nr_php_is_zval_valid_integer(retval)) {
    response_code = Z_LVAL_P(retval);
  }

  nr_php_zval_free(&retval);

  return response_code;
}

/*
 * Wrap and record external metrics for Zend_Http_Client::request.
 * Wrap and record external metrics for Laminas_Http_Client::send.
 *
 * http://framework.zend.com/manual/1.12/en/zend.http.client.advanced.html
 */
NR_PHP_WRAPPER_START(nr_zend_http_client_request) {
  zval* this_var = NULL;
  zval** retval_ptr;
  nr_segment_t* segment;
  nr_segment_external_params_t external_params = {.library = http_client};
  nr_zend_http_adapter adapter;

  (void)wraprec;

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  retval_ptr = nr_php_get_return_value_ptr(TSRMLS_C);

  /* Avoid double counting if CURL is used. */
  adapter = nr_zend_check_adapter(this_var TSRMLS_CC);
  if ((NR_ZEND_ADAPTER_CURL == adapter)
      || (NR_ZEND_ADAPTER_UNKNOWN == adapter)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  if (NR_FAILURE
      == nr_zend_http_client_request_get_url(this_var,
                                             &external_params.uri TSRMLS_CC)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  segment = nr_segment_start(NRPRG(txn), NULL, NULL);

  /*
   * We have to manually force this segment as the current segment on
   * the transaction, otherwise the previously forced stacked segment
   * will be used as parent for segments that should rather be
   * parented to this segment.
   *
   * This solution of purely for Zend_Http_Client issues related to older
   * versions of the Zend framework.
   *
   * Our agent creates an external segment for calls to `request` method calls
   * of `Zend_Http_Client` objects or for calls to `send` method calls
   * of `Laminas_Http_Client` objects. However, the request method itself
   * creates child segments to the external segments triggered by our automatic
   * instrumentation. These child segments are not correctly parented, to the
   * external segment, but to the segment for the `request` method. Thus the
   * total time calculated from those segments is incorrect. This behavior is
   * related to the stacked segment implementation in the agent. This
   * implementation relies on the fact that segments created in the agent
   * explicitly do not have children created by automatic instrumentation.
   * Implement a solution just for `Zend_Http_Client` (e. g. deleting all child
   * segments from the `request` related segment).
   */
  NRTXN(force_current_segment) = segment;

  nr_zend_http_client_request_add_request_headers(this_var, segment TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  if (retval_ptr) {
    external_params.encoded_response_header
        = nr_zend_http_client_request_get_response_header(
            *retval_ptr TSRMLS_CC);
    external_params.status
        = nr_zend_http_client_request_get_response_code(*retval_ptr TSRMLS_CC);
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: unable to obtain return value from request",
                     library_name);
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(NRL_CAT,
                     "CAT: outbound response: transport='%s' %s=" NRP_FMT,
                     http_client, X_NEWRELIC_APP_DATA,
                     NRP_CAT(external_params.encoded_response_header));
  }

  /*
   * We don't want to have Zend_Http_Client request segments to have any
   * children, as this would scramble the exclusive time calculation.
   *
   * Therefore we delete all children of the segment. Afterwards we set
   * the forced current of the transaction back to the segments parent,
   * thus restoring the stacked segment stack.
   */

  if (segment) {
    for (size_t i = 0; i < nr_segment_children_size(&segment->children); i++) {
      nr_segment_t* child = nr_segment_children_get(&segment->children, i);
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: deleting child from %s",
                       library_name, http_client_request);

      nr_segment_discard(&child);
    }
    NRTXN(force_current_segment) = segment->parent;
  }

  nr_segment_external_end(&segment, &external_params);

leave:
  nr_free(external_params.encoded_response_header);
  nr_free(external_params.uri);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_zend_http_enable(TSRMLS_D) {
  if ((NR_FW_ZEND != NRPRG(current_framework))
      && (NR_FW_LAMINAS3 != NRPRG(current_framework))) {
    nr_php_wrap_user_function(NR_PSTR(http_client_request),
                              nr_zend_http_client_request TSRMLS_CC);
  }
}

void nr_laminas_http_enable(TSRMLS_D) {
  if ((NR_FW_ZEND != NRPRG(current_framework))) {
    /*
     * Redefine zend to laminas.
     */
    library_name = LIB_NAME_L;
    curl_adapter_typename = CURL_ADAPTER_L;
    uri_http_typename = URI_HTTP_L;
    http_client = HTTP_CLIENT_L;
    http_client_request = HTTP_CLIENT_REQUEST_L;

    nr_php_wrap_user_function(NR_PSTR(http_client_request),
                              nr_zend_http_client_request TSRMLS_CC);
  }
}

