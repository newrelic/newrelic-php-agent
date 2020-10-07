/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LIB_ZEND_HTTP_HDR
#define LIB_ZEND_HTTP_HDR

#include "php_wrapper.h"

/*
 * Purpose : Wrapper for Zend_Http_Client::request that implements support
 *           for external metrics, CAT, and Synthetics.
 */
extern NR_PHP_WRAPPER_PROTOTYPE(nr_zend_http_client_request);

#endif /* LIB_ZEND_HTTP_HDR */
