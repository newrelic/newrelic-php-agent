/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Utility functions for PSR-7 HTTP message objects.
 *
 * Spec: http://www.php-fig.org/psr/psr-7/
 */
#ifndef PHP_PSR7_HDR
#define PHP_PSR7_HDR

/*
 * Purpose : Test if the given zval is a valid PSR-7 message.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid message; zero otherwise.
 */
static inline int nr_php_psr7_is_message(zval* z TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      z, "Psr\\Http\\Message\\MessageInterface" TSRMLS_CC);
}

/*
 * Purpose : Test if the given zval is a valid PSR-7 request.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid request; zero otherwise.
 */
static inline int nr_php_psr7_is_request(zval* z TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      z, "Psr\\Http\\Message\\RequestInterface" TSRMLS_CC);
}

/*
 * Purpose : Test if the given zval is a valid PSR-7 response.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid response; zero otherwise.
 */
static inline int nr_php_psr7_is_response(zval* z TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      z, "Psr\\Http\\Message\\ResponseInterface" TSRMLS_CC);
}

/*
 * Purpose : Test if the given zval is a valid PSR-7 URI.
 *
 * Params  : 1. The zval to test.
 *
 * Returns : Non-zero if the zval is a valid URI; zero otherwise.
 */
static inline int nr_php_psr7_is_uri(zval* z TSRMLS_DC) {
  return nr_php_object_instanceof_class(
      z, "Psr\\Http\\Message\\UriInterface" TSRMLS_CC);
}

/*
 * Purpose : Get a header from a PSR-7 message object.
 *
 * Params  : 1. The message object.
 *           2. The header name.
 *
 * Returns : The value of the header, or NULL if the message is invalid or the
 *           header doesn't exist.
 *
 * Note    : If multiple headers were provided with the same name, only the
 *           last header is returned.
 */
extern char* nr_php_psr7_message_get_header(zval* message,
                                            const char* name TSRMLS_DC);

/*
 * Purpose : Get the URI for a PSR-7 request.
 *
 * Params  : 1. The request object.
 *
 * Returns : The URI, or NULL if the request or URI is invalid.
 */
extern char* nr_php_psr7_request_uri(zval* request TSRMLS_DC);

#endif /* PHP_PSR7_HDR */
