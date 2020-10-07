/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef PHP_AGENT_FW_SYMFONY_COMMON_H
#define PHP_AGENT_FW_SYMFONY_COMMON_H

/*
 * Purpose : Helper to handle the nitty gritty of naming a transaction based on
 *           the string value of a zval.
 *
 * Params  : 1. A zval used to name the transaction.
 *           2. String containing the Symfony version being used.
 *
 * Return  : NR_SUCCESS = 0 or NR_FAILURE = -1.
 */
int nr_symfony_name_the_wt_from_zval(const zval* name TSRMLS_DC,
                                     const char* symfony_version);

/*
 * Purpose : Call the get method on the given object and return a string zval
 *           if a valid string was returned. The result must be freed.
 *
 * Params  : 1. object the get method is called on
 *           2. string paramater containing what to get
 *
 * Return  : The returned string of nr_php_call(), or NULL if the function 
 *           invocation failed
 */
zval* nr_symfony_object_get_string(zval* obj, const char* param TSRMLS_DC);

#endif /* PHP_AGENT_FW_SYMFONY_COMMON_H */
