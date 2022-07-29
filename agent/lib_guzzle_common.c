/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "php_execute.h"
#include "php_globals.h"
#include "lib_guzzle_common.h"
#include "lib_guzzle4.h"
#include "lib_guzzle6.h"
#include "fw_laravel.h"
#include "fw_laravel_queue.h"
#include "fw_support.h"
#include "php_error.h"
#include "nr_header.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

int php_version_compare(char*, char*);

char* nr_guzzle_create_async_context_name(const char* prefix, const zval* obj) {
  if (!nr_php_is_zval_valid_object(obj)) {
    return NULL;
  }

  return nr_formatf("%s #%d", prefix, Z_OBJ_HANDLE_P(obj));
}

static int nr_guzzle_stack_iterator(zval* frame,
                                    int* in_guzzle_ptr,
                                    zend_hash_key* key NRUNUSED TSRMLS_DC) {
  int idx;
  zval* klass = NULL;

  NR_UNUSED_TSRMLS;

  if (0 == nr_php_is_zval_valid_array(frame)) {
    return ZEND_HASH_APPLY_KEEP;
  }
  if (NULL == in_guzzle_ptr) {
    return ZEND_HASH_APPLY_KEEP;
  }

  klass = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "class");

  if (0 == nr_php_is_zval_non_empty_string(klass)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  idx = nr_strncaseidx(Z_STRVAL_P(klass), "guzzle", Z_STRLEN_P(klass));
  if (idx >= 0) {
    *in_guzzle_ptr = 1;
  }

  return ZEND_HASH_APPLY_KEEP;
}

int nr_guzzle_in_call_stack(TSRMLS_D) {
  int in_guzzle = 0;
  zval* stack = NULL;

  if (0 == NRINI(guzzle_enabled)) {
    return 0;
  }

  stack = nr_php_backtrace(TSRMLS_C);

  if (nr_php_is_zval_valid_array(stack)) {
    nr_php_zend_hash_zval_apply(Z_ARRVAL_P(stack),
                                (nr_php_zval_apply_t)nr_guzzle_stack_iterator,
                                &in_guzzle TSRMLS_CC);
  }

  nr_php_zval_free(&stack);

  return in_guzzle;
}

extern char* nr_guzzle_version(zval* obj TSRMLS_DC) {
  char* retval = NULL;
  zval* version = NULL;
  zend_class_entry* ce = NULL;

  if (0 == nr_php_is_zval_valid_object(obj)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application object is invalid",
                     __func__);
    return NULL;
  }

  ce = Z_OBJCE_P(obj);
  if (NULL == ce) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application has NULL class entry",
                     __func__);
    return NULL;
  }

  if (nr_php_get_class_constant(ce, "VERSION") == NULL){
    version = nr_php_get_class_constant(ce, "MAJOR_VERSION");
  } else {
    version = nr_php_get_class_constant(ce, "VERSION");
  }
  if (NULL == version) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application does not have VERSION",
                     __func__);
    return NULL;
  }

  if (nr_php_is_zval_valid_string(version)) {
    retval = nr_strndup(Z_STRVAL_P(version), Z_STRLEN_P(version));
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: expected VERSION be a valid string, got type %d",
                     __func__, Z_TYPE_P(version));
  }
  nr_php_zval_free(&version);
  return retval;
}

nr_segment_t* nr_guzzle_obj_add(const zval* obj,
                                const char* async_context_prefix TSRMLS_DC) {
  nr_segment_t* segment = NULL;
  char* async_context = NULL;

  /*
   * Create the async context, in case there was parallelism.
   */
  async_context
      = nr_guzzle_create_async_context_name(async_context_prefix, obj);

  segment = nr_segment_start(NRPRG(txn), NULL, async_context);

  nr_free(async_context);

  /*
   * Create the guzzle_objs hash table if we haven't already done so.
   */
  if (NULL == NRTXNGLOBAL(guzzle_objs)) {
    NRTXNGLOBAL(guzzle_objs) = nr_hashmap_create(NULL);
  }

  /*
   * We store the start times indexed by the object handle for the Request
   * object: Zend object handles are unsigned ints while HashTable objects
   * support unsigned longs as indexes, so this is safe regardless of
   * architecture, and saves us having to transform the object handle into a
   * string to use string keys.
   */
  nr_hashmap_index_update(NRTXNGLOBAL(guzzle_objs),
                          (uint64_t)Z_OBJ_HANDLE_P(obj), segment);

  return segment;
}

nr_status_t nr_guzzle_obj_find_and_remove(const zval* obj,
                                          nr_segment_t** segment_ptr
                                              TSRMLS_DC) {
  if (NULL != NRTXNGLOBAL(guzzle_objs)) {
    uint64_t index = (uint64_t)Z_OBJ_HANDLE_P(obj);
    nr_segment_t* segment = NULL;

    segment
        = (nr_segment_t*)nr_hashmap_index_get(NRTXNGLOBAL(guzzle_objs), index);
    *segment_ptr = segment;

    if (segment) {
      /*
       * Remove the object handle from the hashmap containing active requests.
       */
      nr_hashmap_index_delete(NRTXNGLOBAL(guzzle_objs), index);

      return NR_SUCCESS;
    }
  }

  nrl_verbosedebug(NRL_INSTRUMENT,
                   "Guzzle: object %d not found in tracked list",
                   Z_OBJ_HANDLE_P(obj));
  return NR_FAILURE;
}

/*
 * Purpose : Sets a header on an object implementing either the Guzzle 3 or 4
 *           MessageInterface.
 *
 * Params  : 1. The header to set.
 *           2. The value to set the header to.
 *           3. The request object.
 */
static void nr_guzzle_request_set_header(const char* header,
                                         const char* value,
                                         zval* request TSRMLS_DC) {
  zval* header_param = NULL;
  zval* retval = NULL;
  zval* value_param = NULL;

  if ((NULL == header) || (NULL == value) || (NULL == request)) {
    return;
  }

  header_param = nr_php_zval_alloc();
  nr_php_zval_str(header_param, header);
  value_param = nr_php_zval_alloc();
  nr_php_zval_str(value_param, value);

  retval = nr_php_call(request, "setHeader", header_param, value_param);

  nr_php_zval_free(&header_param);
  nr_php_zval_free(&retval);
  nr_php_zval_free(&value_param);
}

void nr_guzzle_request_set_outbound_headers(zval* request,
                                            nr_segment_t* segment TSRMLS_DC) {
  nr_hashmap_t* outbound_headers = NULL;
  nr_vector_t* header_keys = NULL;
  char* header = NULL;
  char* value = NULL;
  size_t i;
  size_t header_count;

  outbound_headers = nr_header_outbound_request_create(NRPRG(txn), segment);

  if (NULL == outbound_headers) {
    return;
  }

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT,
        "CAT: outbound request: transport='Guzzle' %s=" NRP_FMT " %s=" NRP_FMT,
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
    nr_guzzle_request_set_header(header, value, request TSRMLS_CC);
  }

  nr_vector_destroy(&header_keys);
  nr_hashmap_destroy(&outbound_headers);
}

char* nr_guzzle_response_get_header(const char* header,
                                    zval* response TSRMLS_DC) {
  zval* param = nr_php_zval_alloc();
  zval* retval = NULL;
  char* value = NULL;

  nr_php_zval_str(param, header);

  retval = nr_php_call(response, "getHeader", param);
  if (NULL == retval) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle: Response::getHeader() returned NULL");
  } else if (nr_php_is_zval_valid_string(retval)) {
    /*
     * Guzzle 4 and 5 return an empty string if the header could not be found.
     */
    if (Z_STRLEN_P(retval) > 0) {
      value = nr_strndup(Z_STRVAL_P(retval), Z_STRLEN_P(retval));
    }
  } else if (nr_php_object_instanceof_class(
                 retval, "Guzzle\\Http\\Message\\Header" TSRMLS_CC)) {
    /*
     * Guzzle 3 returns an object that we can cast to a string, so let's do
     * that. We'll call __toString() directly rather than going through PHP's
     * convert_to_string() function, as that will generate a notice if the
     * cast fails for some reason.
     */
    zval* zv_str = nr_php_call(retval, "__toString");

    if (nr_php_is_zval_non_empty_string(zv_str)) {
      value = nr_strndup(Z_STRVAL_P(zv_str), Z_STRLEN_P(zv_str));
    } else if (NULL != zv_str) {
      nrl_verbosedebug(
          NRL_INSTRUMENT,
          "Guzzle: Header::__toString() returned a non-string of type %d",
          Z_TYPE_P(zv_str));
    } else {
      /*
       * We should never get NULL as the retval from nr_php_call, but handle it
       * just in case...
       */
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "Guzzle: Header::__toString() returned a NULL retval");
    }

    nr_php_zval_free(&zv_str);
  } else {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Guzzle: unexpected Response::getHeader() return of type %d",
        Z_TYPE_P(retval));
  }

  nr_php_zval_free(&param);
  nr_php_zval_free(&retval);

  return value;
}

NR_PHP_WRAPPER_START(nr_guzzle_client_construct) {
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  char *version = NULL;
  version = nr_guzzle_version(this_var TSRMLS_CC);

  (void)wraprec;
  NR_UNUSED_SPECIALFN;
  nr_php_scope_release(&this_var);
  
  if (php_version_compare(version, "7") >= 0){
    NR_PHP_WRAPPER_DELEGATE(nr_guzzle7_client_construct);
  } else if (php_version_compare(version, "6") >= 0) {
    NR_PHP_WRAPPER_DELEGATE(nr_guzzle6_client_construct);
  } else{
    NR_PHP_WRAPPER_DELEGATE(nr_guzzle4_client_construct);
  }
}
NR_PHP_WRAPPER_END
