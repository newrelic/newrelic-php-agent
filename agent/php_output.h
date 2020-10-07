/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Utility functions to work with PHP's output subsystem.
 */
#ifndef PHP_OUTPUT_HDR
#define PHP_OUTPUT_HDR

/*
 * Purpose : Test if the output handler flags indicate that there is content
 *           that should be read.
 *
 * Params  : 1. The output handler flags (in PHP 5.3, this is the "mode").
 *
 * Returns : Non-zero if there is content; zero otherwise.
 */
extern int nr_php_output_has_content(int flags);

/*
 * Purpose : PHP version agnostic way to create a new output buffer with
 *           a handler.  This is the PHP internal equivalent to calling
 *           ob_start (http://php.net/manual/en/function.ob-start.php).
 *
 * Params  : 1. The name of the output buffer, which is user-visible via
 *              ob_list_handlers().
 *           2. The output handler function.
 *
 * Warning : The meaning of the mode argument that is given to the output
 *           handler changed significantly in PHP 5.4.
 */
extern void nr_php_output_install_handler(const char* name,
                                          php_output_handler_func_t handler
                                              TSRMLS_DC);

/*
 * Purpose : Test if the output handler flags indicate that this is the first
 *           chunk of content.
 *
 * Params  : 1. The output handler flags (in PHP 5.3, this is the "mode").
 *
 * Returns : Non-zero if this is the first chunk; zero otherwise.
 */
extern int nr_php_output_is_end(int flags);

/*
 * Purpose : Test if the output handler flags indicate that this is the last
 *           chunk of content.
 *
 * Params  : 1. The output handler flags (in PHP 5.3, this is the "mode").
 *
 * Returns : Non-zero if this is the last chunk; zero otherwise.
 */
extern int nr_php_output_is_start(int flags);

#endif /* PHP_OUTPUT_HDR */
