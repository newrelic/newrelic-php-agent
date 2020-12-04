/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file declares all of the replacement PHP hook functions.
 */
#ifndef PHP_HOOKS_HDR
#define PHP_HOOKS_HDR

/*
 * Purpose : Pretty much what the entire agent is about:
 *           the hook for zend_execute    (PHP <= 5.4)
 *           the hook for zend_execute_ex (PHP >= 5.5)
 *           See http://www.php.net/manual/en/migration55.internals.php
 *           for a discussion of what/how to override.
 */
extern void nr_php_execute(NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Our own error callback function, used to capture the PHP stack
 *           trace. This function is bound to zend_error_cb, and is typically
 *           called from within the guts of zend_error.
 *
 * Params  : 1. A bitset encoding the type of the error, taken from
 *           E_ERROR ... E_USER_DEPRECATED
 *           2. The filename
 *           3. The line number the error occurred on.
 *           4. The format type of the error message.
 *           5. The args that combine to get the error message.
 *
 * Notes:
 *
 * PHP8 changed how this is handled.
 * Error notifications have a new API.  Instead of overwriting zend_error_cb
 * extensions with debugging, monitoring use-cases catching Errors/Exceptions
 * are strongly encouraged to use the new error notification API instead.
 * Error notification callbacks are guaranteed to be called regardless of the
 * users error_reporting setting or userland error handler return values.
 * Register notification callbacks during MINIT of an extension:
void my_error_notify_cb(int type,
                const char *error_filename,
                        uint32_t error_lineno,
                        zend_string *message) {
                }
                zend_register_error_notify_callback(my_error_notify_cb);
 */
#if ZEND_MODULE_API_NO <= ZEND_8_0_X_API_NO
extern void nr_php_error_cb(int type,
                            const char* filename,
                            uint error_lineno,
                            zend_string* message);
#else
extern void nr_php_error_cb(int type,
                            const char* filename,
                            uint error_lineno,
                            const char* fmt,
                            va_list args)
    ZEND_ATTRIBUTE_PTR_FORMAT(printf, 4, 0);
#endif /* PHP <= 8.0 */

#if ZEND_MODULE_API_NO >= ZEND_7_0_X_API_NO /* PHP 7.0+ */
extern void nr_php_execute_internal(zend_execute_data* execute_data,
                                    zval* return_value);
#elif ZEND_MODULE_API_NO >= ZEND_5_5_X_API_NO
extern void nr_php_execute_internal(zend_execute_data* execute_data,
                                    zend_fcall_info* fci,
                                    int return_value_used TSRMLS_DC);
#else
extern void nr_php_execute_internal(zend_execute_data* execute_data,
                                    int return_value_used TSRMLS_DC);
#endif

#endif /* PHP_HOOKS_HDR */
