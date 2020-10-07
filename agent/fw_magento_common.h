/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions common to multiple versions of Magento.
 */
#ifndef FW_MAGENTO_COMMON_HDR
#define FW_MAGENTO_COMMON_HDR

/*
 * Purpose : Name the current transaction based on the given action.
 *
 * Params  : 1. An action object that implements a getRequest() method, which in
 *              turn returns a request object that implements getModuleName(),
 *              getControllerName() and getActionName() methods.
 */
extern void nr_magento_name_transaction(zval* action TSRMLS_DC);

#endif /* FW_MAGENTO_COMMON_HDR */
