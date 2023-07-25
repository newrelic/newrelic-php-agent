/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions common to both Drupal frameworks.
 *
 * We support both Drupal 6/7 (FW_DRUPAL) and Drupal 8/9 (FW_DRUPAL8) within the
 * agent. These framework versions are significantly different internally and
 * have hence been implemented as separate frameworks, but share some code.
 */
#ifndef FW_DRUPAL_COMMON_HDR
#define FW_DRUPAL_COMMON_HDR

#include "php_user_instrument.h"

#define NR_DRUPAL_MODULE_PREFIX "Framework/Drupal/Module/"
#define NR_DRUPAL_HOOK_PREFIX "Framework/Drupal/Hook/"
#define NR_DRUPAL_VIEW_PREFIX "Framework/Drupal/ViewExecute/"

/*
 * Purpose : Create a Drupal metric.
 *
 * Params  : 1. The segment to create the metric on.
 *           2. The prefix to use when creating the metric.
 *           3. The length of the prefix.
 *           4. The suffix to use when creating the metric.
 *           5. The length of the suffix.
 */
extern void nr_drupal_create_metric(nr_segment_t* segment,
                                    const char* prefix,
                                    int prefix_len,
                                    const char* suffix,
                                    int suffix_len);

/*
 * Purpose : Call the original Drupal view execute function and create the
 *           appropriate view metric.
 *
 * Params  : 1. The view name.
 *           2. The length of the view name.
 *           3. The function segment.
 *           4. The original execute data.
 *
 * Returns : Non-zero if zend_bailout needs to be called.
 */
extern int nr_drupal_do_view_execute(const char* name,
                                     int name_len,
                                     nr_segment_t* segment,
                                     NR_EXECUTE_PROTO TSRMLS_DC);

/*
 * Purpose : Determine whether the given framework is a Drupal framework.
 *
 * Params  : 1. The framework to test.
 *
 * Returns : Non-zero if the framework is Drupal.
 */
extern int nr_drupal_is_framework(nrframework_t fw);

/*
 * Purpose : Wrap a user function with Drupal module and hook metadata.
 *
 * Params  : 1. The function name.
 *           2. The length of the function name.
 *           3. The module name.
 *           4. The length of the module name.
 *           5. The hook name.
 *           6. The length of the hook name.
 *
 * Returns : The user function wrapper, or NULL on failure.
 */
extern nruserfn_t* nr_php_wrap_user_function_drupal(const char* name,
                                                    int namelen,
                                                    const char* module,
                                                    nr_string_len_t module_len,
                                                    const char* hook,
                                                    nr_string_len_t hook_len
                                                        TSRMLS_DC);

/*
 * Purpose : Instrument the given module and hook.
 *
 * Params  : 1. The module name.
 *           2. The module name length.
 *           3. The hook name.
 *           4. The hook name length.
 */
extern void nr_drupal_hook_instrument(const char* module,
                                      size_t module_len,
                                      const char* hook,
                                      size_t hook_len TSRMLS_DC);

/*
 * Purpose : Given a function that is a hook function in a module, determine
 *           which component is the module and which is the hook, given that we
 *           know the hook from the module_invoke_all() call.
 *
 *           This function's job is to accept all the information we know,
 *           and extract the function name/length from the zend_function
 *
 * Params  : 1. A "pointer as return value" for the module name
 *           2. A "pointer as return value" for the module name lenggth
 *           3. The hook name, which we know from the call to module_invoke_all
 *           4. The length of the hook name
 *           5. The zend_function object representing the PHP hook function
 */
nr_status_t module_invoke_all_parse_module_and_hook(char** module_ptr,
                                                    size_t* module_len_ptr,
                                                    const char* hook,
                                                    size_t hook_len,
                                                    const zend_function* func);
/*
 * Purpose : Given a function that is a hook function in a module, determine
 *           which component is the module and which is the hook, given that we
 *           know the hook from the module_invoke_all() call.
 *
 *           This function is called by module_invoke_all_parse_module_and_hook
 *           and implements the algorithm for extracting the module name
 *
 * Params  : 1. A "pointer as return value" for the module name
 *           2. A "pointer as return value" for the module name lenggth
 *           3. The hook name, which we know from the call to module_invoke_all
 *           4. The name of the PHP hook function
 *           5. The length of the name of the PHP hook function
 */
nr_status_t module_invoke_all_parse_module_and_hook_from_strings(
    char** module_ptr,
    size_t* module_len_ptr,
    const char* hook,
    size_t hook_len,
    const char* module_hook,
    size_t module_hook_len);

/*
 * Purpose: This function adds NR request headers for Drupal. arg is the second
 *          argument given to drupal_http_request. arg can either be:
 *
 *           - an array with request header key/value pairs for Drupal 6
 *           - an options array, in which the value for the key 'headers' is an
 *             array with request header key/value pairs
 *
 * Params  : 1. A zval holding the second argument given to drupal_http_request
 *           2. true if headers should be added for Drupal 7 or newer, false
 *              it headers should be added for Drupal 6 or older.
 */
void nr_drupal_headers_add(zval* arg, bool is_drupal_7 TSRMLS_DC);


#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
/*
 * Purpose: Before an invoke_all style call, adds the hook to that hook states stacks
 *
 * Params  : 1. A zval holding a copy of the hook invoked, to be managed by the hook
 *              states stacks and freed by invoke_all_clean_hook_stacks() after the
 *              invoke_all call completes
 */
void invoke_all_push_hook_stacks(zval* hook_copy);

/*
 * Purpose: After an invoke_all style call, cleans that hook states stacks
 */
void invoke_all_clean_hook_stacks();
#endif // OAPI

#endif /* FW_DRUPAL_COMMON_HDR */
