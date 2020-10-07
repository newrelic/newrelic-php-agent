/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for dealing with request/response headers.
 */
#ifndef PHP_HEADER_HDR
#define PHP_HEADER_HDR

/*
 * Purpose : Returns the given request header.
 *
 * Params  : 1. The header name, in the form it appears in the $_SERVER
 *              superglobal (eg HTTP_X_NEWRELIC_ID for X-NewRelic-Id).
 *
 * Returns : The header value, or NULL if the header doesn't exist. The caller
 *           is responsible for freeing the value.
 */
extern char* nr_php_get_request_header(const char* name TSRMLS_DC);

/*
 * Purpose : Determines whether in the incoming request has a header of the
 *           given name.
 *
 * Returns : 1 if the header is present, and 0 otherwise.
 */
extern int nr_php_has_request_header(const char* name TSRMLS_DC);

/*
 * Purpose : Determines whether the response headers have content length set.
 *
 * Returns : 1 if the header is present, and 0 otherwise.
 */
extern int nr_php_has_response_content_length(TSRMLS_D);

/*
 * Purpose : Return a copy of the mimetype for the current response.
 *
 * Returns : A valid string. The caller is responsible for freeing the return
 *           value.
 */
extern char* nr_php_get_response_content_type(TSRMLS_D);

/*
 * Purpose : Return the content length for the current response.
 *
 * Returns : The content length as an integer or -1 if the header is missing or
 *           invalid.
 */
extern int nr_php_get_response_content_length(TSRMLS_D);

/*
 * Purpose : Output buffer handler of type php_output_handler_func_t designed
 *           to add the cross process response header.
 *           See: nr_php_install_output_buffer_handler
 *
 *           This output buffer handler does not modify its output.  Instead,
 *           it is used to identify the proper time to create the cross
 *           process response header.  This header creation should be
 *           delayed as long as possible, since it contains a duration which
 *           should be as close as possible to the actual txn's duration.
 *           Unfortunately, RSHUTDOWN is too late and the response headers have
 *           already been sent.  The AutoRUM buffer could be re-used for this
 *           purpose, however, this approach was taken for simplicity.
 *
 *           This buffer handler does not need to be added if a cross process
 *           request header is not present.  This is an optimization, to avoid
 *           adding the handler when it will definitely not be needed.
 *
 *           This buffer handler should not be added if cross process is
 *           disabled.  Thus, by turning off cross process, the user can
 *           ensure that this buffer will not be present.  This is useful
 *           in buffering problem circumstances.
 */
extern void nr_php_header_output_handler(
    char* output NRUNUSED,
    nr_output_buffer_string_len_t output_len NRUNUSED,
    char** handled_output,
    nr_output_buffer_string_len_t* handled_output_len NRUNUSED,
    int mode TSRMLS_DC);
/*
 * Purpose : Wrap the SAPI module's header handler so we can capture the
 *           a pointer to SG(sapi_headers).
 */
extern int nr_php_header_handler(sapi_header_struct* sapi_header,
                                 sapi_header_op_enum op,
                                 sapi_headers_struct* sapi_headers TSRMLS_DC);

/*
 * Purpose : Provide safe access to SG(sapi_headers).
 *
 * Returns : A pointer to the sapi_headers field of the SAPI globals.
 */
extern sapi_headers_struct* nr_php_sapi_headers(TSRMLS_D);

/*
 * Purpose : Provide safe access to the response headers for the current
 * request.
 *
 * Returns : A pointer to a linked list containing the response headers.
 */
extern zend_llist* nr_php_response_headers(TSRMLS_D);

/*
 * Purpose : Call sapi_header_op() to force our wrapper of the SAPI header
 *           handler to be invoked. This ensures we get a pointer to
 *           SG(sapi_headers).
 */
extern void nr_php_capture_sapi_headers(TSRMLS_D);

/*
 * Purpose : Provide safe access to the http response code for the current
 * request.
 *
 * Returns : The http response code as an integer.
 */
extern int nr_php_http_response_code(TSRMLS_D);

#endif /* PHP_HEADER_HDR */
