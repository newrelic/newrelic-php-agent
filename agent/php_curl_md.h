/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_CURL_MD_HDR
#define PHP_CURL_MD_HDR

#include "util_time.h"
#include "util_vector.h"

typedef struct _nr_php_curl_md_t {
  char* method;
  zval* outbound_headers;
  char* response_header;
  nr_segment_t* segment;
  nrtime_t txn_start_time; /* Time at which the associated segment's parent
                             transaction was created. Used in detection of
                             transaction restarts in between multi_execs */
} nr_php_curl_md_t;

typedef struct _nr_php_curl_multi_md_t {
  nr_vector_t curl_handles; /* A vector of single curl handles added to this
                               multi handle */
  nr_segment_t* segment;    /* The segment representing the multi handle */
  char* async_context; /* The async context name, shared by the multi handle
                          with the single handles added to it */
  bool initialized; /* Whether this metadata struct was initialized. Set on the
                       first call to curl_multi_exec. */
  nrtime_t txn_start_time; /* Time at which the associated segment's parent
                             transaction was created. Used in detection of
                             transaction restarts in between multi_execs */
} nr_php_curl_multi_md_t;

/*
 * Purpose : Retrieves a pointer to the nr_php_curl_md_t struct associated with
 *           the given curl handle
 *
 * Params  : 1. A curl handle zval
 *
 * Returns : Pointer to the metadata struct or NULL if curl handle is invalid
 */
extern const nr_php_curl_md_t* nr_php_curl_md_get(const zval* ch TSRMLS_DC);

/*
 * Purpose : Sets the method field of the metadata struct associated with
 *           the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *           2. A string containing the method name
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_md_set_method(const zval* ch,
                                      const char* method TSRMLS_DC);

/*
 * Purpose : Retrieves the method field of the metadata struct associated with
 *           the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *
 * Returns : method string if set, otherwise default value of "GET"
 */
extern const char* nr_php_curl_md_get_method(const zval* ch TSRMLS_DC);

/*
 * Purpose : Sets the outbound_headers field of the metadata struct associated
 *           with the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *           2. A zval array representing the headers in (HeaderName => value)
 *              format
 *
 * Returns : true if the operation was successful, otherwise false
 */
extern bool nr_php_curl_md_set_outbound_headers(const zval* ch,
                                                zval* headers TSRMLS_DC);

/*
 * Purpose : Sets the response_header field of the metadata struct associated
 *           with the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *           2. The header text as a c-string
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_md_set_response_header(const zval* ch,
                                               const char* header TSRMLS_DC);

/*
 * Purpose : Retrieves the response_header field of the metadata struct
 *           associated with the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *
 * Returns : response_header string if set, otherwise NULL
 */
extern const char* nr_php_curl_md_get_response_header(const zval* ch TSRMLS_DC);

/*
 * Purpose : Sets the segment field of the metadata struct associated
 *           with the curl handle passed
 *
 * Params  : 1. The associated curl handle zval
 *           2. The segment associated to this curl handle
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_md_set_segment(zval* ch,
                                       nr_segment_t* segment TSRMLS_DC);

/*
 * Purpose : Retrieves the segment field of the metadata struct
 *           associated with the curl handle passed in
 *
 * Params  : 1. The associated curl handle zval
 *
 * Returns : The segment associated to this curl handle if set, otherwise NULL.
 */
extern nr_segment_t* nr_php_curl_md_get_segment(const zval* ch TSRMLS_DC);

/*
 * Purpose : Retrieves a pointer to the nr_php_curl_multi_md_t struct associated
 *           with the given curl multi handle
 *
 * Params  : 1. A curl multi handle zval
 *
 * Returns : Pointer to the curl multi metadata struct or NULL if curl multi
 *           handle is invalid
 */
extern nr_php_curl_multi_md_t* nr_php_curl_multi_md_get(
    const zval* mh TSRMLS_DC);

/*
 * Purpose : Adds the associated curl handle to the nr_php_curl_multi_md_t
 *           struct
 *
 * Params  : 1. The associated curl metadata
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_multi_md_add(const zval* mh, zval* ch TSRMLS_DC);

/*
 * Purpose : Removes the associated curl handle from the nr_php_curl_multi_md_t
 *           struct
 *
 * Params  : 1. The associated curl metadata
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_multi_md_remove(const zval* mh,
                                        const zval* ch TSRMLS_DC);

/*
 * Purpose : Sets the segment field of the metadata struct associated
 *           with the curl multi handle passed
 *
 * Params  : 1. The associated curl multi handle zval
 *           2. The segment associated to this curl multi handle
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_multi_md_set_segment(zval* mh,
                                             nr_segment_t* segment TSRMLS_DC);

/*
 * Purpose : Retrieves the segment field of the metadata struct
 *           associated with the curl multi handle passed in
 *
 * Params  : 1. The associated curl multi handle zval
 *
 * Returns : The segment associated to this curl multi handle if set, otherwise
 *           NULL.
 */
extern nr_segment_t* nr_php_curl_multi_md_get_segment(const zval* mh TSRMLS_DC);

/*
 * Purpose : Retrieves async context name of the metadata struct associated with
 *           the curl multi handle passed in
 *
 * Params  : 1. The associated curl multi handle zval
 *
 * Returns : The name of the async context of the curl multi handle, otherwise
 *           NULL.
 */
extern const char* nr_php_curl_multi_md_get_async_context(
    const zval* mh TSRMLS_DC);

/*
 * Purpose : Retrieves curl single handles associated with the curl multi handle
 *           passed in
 *
 * Params  : 1. The associated curl multi handle zval
 *
 * Returns : A vector containing the associated curl single handles, or NULL.
 */
extern nr_vector_t* nr_php_curl_multi_md_get_handles(const zval* mh TSRMLS_DC);

/*
 * Purpose : Marks the metadata for the curl multi handle as initialized
 *
 * Params  : 1. The associated curl multi handle zval
 *
 * Returns : true upon successful operation, otherwise false
 */
extern bool nr_php_curl_multi_md_set_initialized(const zval* mh TSRMLS_DC);

/*
 * Purpose : Checks if the metadata for the curl multi handle has been
 *           initialized
 *
 * Params  : 1. The associated curl multi handle zval
 *
 * Returns : true if the metadata has been initialized, false otherwise.
 */
extern bool nr_php_curl_multi_md_is_initialized(const zval* mh TSRMLS_DC);

/*
 * Purpose : Performs tasks that we need performed on RSHUTDOWN in the Curl
 *           instrumentation.
 */
extern void nr_curl_rshutdown(TSRMLS_D);

#endif /* PHP_CURL_MD_HDR */
