/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions relating to external calls made using
 * file_get_contents.
 */
#ifndef PHP_FILE_GET_CONTENTS_HDR
#define PHP_FILE_GET_CONTENTS_HDR

/*
 * Purpose : Get the cross process response header directly after a
 *           file_get_contents call.
 */
extern char* nr_php_file_get_contents_response_header(TSRMLS_D);

/*
 * Purpose : Add or remove outbound cross process headers from a stream context
 *           resource.  This is done before and after a file_get_contents
 *           call so that our cross process headers do not accumulate in a
 *           context.  Additionally, if cross process headers were left in
 *           a context, they would become invalid if the transaction or
 *           application were to change.
 *
 * Params  : 1. The stream context resource.
 *
 * Returns : Nothing.
 *
 * Notes   : These functions are exported as PHP functions to facilitate
 *           testing.
 */
extern void nr_php_file_get_contents_remove_headers(zval* context TSRMLS_DC);
extern void nr_php_file_get_contents_add_headers(zval* context,
                                                 nr_segment_t* segment
                                                     TSRMLS_DC);
extern PHP_FUNCTION(newrelic_add_headers_to_context);
extern PHP_FUNCTION(newrelic_remove_headers_from_context);

extern zval* nr_php_file_get_contents_get_method(zval* context TSRMLS_DC);
/*
 * Purpose : Calls file_get_contents with the parameters given as well as
 *           a new context.  The context will allow the instrumentation of
 *           the recursive call to add the cross process request headers.
 */
extern nr_status_t nr_php_file_get_contents_recurse_with_context(
    zval* return_value,
    zval* file_zval,
    zval* use_include_path,
    zval* offset,
    zval* maxlen TSRMLS_DC);

#endif /* PHP_FILE_GET_CONTENTS_HDR */
