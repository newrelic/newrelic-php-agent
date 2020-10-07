/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains functions relating to external calls made using curl.
 */
#ifndef PHP_CURL_HDR
#define PHP_CURL_HDR

/*
 * Purpose : This function is added to the extension to provide a default
 *           curl response header callback.
 */
extern PHP_FUNCTION(newrelic_curl_header_callback);

/*
 * Purpose : Modify a newly created curl resource to support cross process
 *           headers.
 *
 * Params  : 1. A curl resource which has just been created by curl_init.
 */
extern void nr_php_curl_init(zval* curlres TSRMLS_DC);

/*
 * Purpose : Handle anything that should be done for curl external
 *           instrumentation before the original curl_setopt() handler is
 *           invoked. In practice, this means modifying callback parameters
 *           given to curl_setopt() to support cross process headers.
 *
 * Params  : The three parameters to curl_setopt: The curl resource, the option
 *           constant, and the value being set.
 */
extern void nr_php_curl_setopt_pre(zval* curlres,
                                   zval* curlopt,
                                   zval* curlval TSRMLS_DC);

/*
 * Purpose : Handle anything that should be done for curl external
 *           instrumentation after the original curl_setopt() handler is
 *           invoked. In practice, this means re-calling curl_setopt() if the
 *           user was setting headers via CURLOPT_HTTPHEADER.
 *
 * Params  : The three parameters to curl_setopt: The curl resource, the option
 *           constant, and the value being set.
 */
extern void nr_php_curl_setopt_post(zval* curlres,
                                    zval* curlopt,
                                    zval* curlval TSRMLS_DC);

typedef void (*nr_php_curl_setopt_func_t)(zval* curlres,
                                          zval* curlopt,
                                          zval* curlval TSRMLS_DC);

/*
 * Purpose : Handle anything that should be done for curl external
 *           instrumentation when curl_setopt_array() is called.
 *
 * Params  : 1. The curl resource.
 *           2. The options array.
 *           3. The function to invoke for each option: in practice, either
 *              nr_php_curl_setopt_pre or nr_php_curl_setopt_post.
 */
extern void nr_php_curl_setopt_array(zval* curlres,
                                     zval* options,
                                     nr_php_curl_setopt_func_t func TSRMLS_DC);

/*
 * Purpose : Get the url of a curl resource.
 *
 * Returns : A newly allocated string containing the url, or NULL on error.
 */
extern char* nr_php_curl_get_url(zval* curlres TSRMLS_DC);

/*
 * Purpose : Get the HTTP status code of a curl resource.
 *
 * Returns : The HTTP status code
 */
extern uint64_t nr_php_curl_get_status_code(zval* curlres TSRMLS_DC);

/*
 * Purpose : Determines whether the url for a curl resource represents a
 *           protocol that should be instrumented by the agent.
 *
 * Params  : 1. A null-terminated string representing the url to test.
 *
 * Returns : Non-zero if the protocol should be instrumented; otherwise, zero.
 */
extern int nr_php_curl_should_instrument_proto(const char* url);

/*
 * Purpose : Start an external segment for a curl resource.
 *
 * Params  : 1. The curl resource.
 *           2. The parent for the external segment. If this is NULL, the
 *              current segment of the current transaction is used as parent.
 *           3. The async_context for the external segment, or NULL to indicate
 *              that the segment is not asynchronous.
 *
 * Notes   : Both parameters parent and async_context can be NULL. This has the
 *           same implications as passing NULL for one of those parameters to
 *           nr_segment_start.
 */
extern void nr_php_curl_exec_pre(zval* curlres,
                                 nr_segment_t* parent,
                                 const char* async_context TSRMLS_DC);

/*
 * Purpose : End an external segment for a curl resource.
 *
 * Params  : 1. The curl resource.
 *           2. If true, the duration of the external segment is set to the
 *              total time as returned by curl_getinfo. If false, the duration
 *              of the external segment is calculated from the current
 *              timestamp.
 */
extern void nr_php_curl_exec_post(zval* curlres,
                                  bool duration_from_handle TSRMLS_DC);

/*
 * Purpose : Start an external segment for a curl multi resource.
 *
 *           This call also starts segments for all curl handles added to the
 *           curl multi resource.
 *
 *           If this function has already been called on the given curl multi
 *           resource, it does nothing.
 *
 * Params  : 1. The curl multi resource.
 */
extern void nr_php_curl_multi_exec_pre(zval* curlres TSRMLS_DC);

/*
 * Purpose : Try to end segments for a curl multi resource.
 *
 *	     This loops over all curl handles added to the curl multi resource
 *	     and ends the related segment if the request associated with the
 *	     curl handle has finished.
 *
 * Params  : 1. The curl multi resource.
 */
extern void nr_php_curl_multi_exec_post(zval* curlres TSRMLS_DC);

/*
 * Purpose : End all segments for a curl multi resource.
 *
 *           This ends all segments of related curl handles that have not been
 *           ended yet. This is the case for handles for which no request could
 *           be made. Those cases aren't caught by nr_php_curl_multi_exec_post,
 *           as curl_getinfo can't tell us about failed requests.
 *
 * Params  : 1. The curl multi resource.
 */
extern void nr_php_curl_multi_exec_finalize(zval* curlres TSRMLS_DC);

#endif /* PHP_CURL_HDR */
