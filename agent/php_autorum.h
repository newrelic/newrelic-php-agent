/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file handles automatic real user monitoring (auto-RUM).
 */
#ifndef PHP_AUTORUM_HDR
#define PHP_AUTORUM_HDR

/*
 * Purpose : Output buffer handler of type php_output_handler_func_t designed
 *           to insert RUM Javascript.
 *           See: nr_php_install_output_buffer_handler
 *
 * Notes   : This buffer should only added if autorum is enabled and the
 *           transaction is a web transaction (not a background task).
 */
extern void nr_php_rum_output_handler(
    char* output,
    nr_output_buffer_string_len_t output_len,
    char** handled_output,
    nr_output_buffer_string_len_t* handled_output_len,
    int mode TSRMLS_DC);

#endif /* PHP_AUTORUM_HDR */
