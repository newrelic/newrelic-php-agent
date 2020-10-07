/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains extension handling functions.
 */
#ifndef PHP_MODULE_HDR
#define PHP_MODULE_HDR

/* Declare the opaque extensions structure. */
struct _nr_php_extensions_t;
typedef struct _nr_php_extensions_t nr_php_extensions_t;

/*
 * Purpose : Allocate and return an extensions structure.
 *
 * Returns : A newly created extensions structure.
 */
nr_php_extensions_t* nr_php_extension_instrument_create(void);

/*
 * Purpose : Instrument all defined Zend extensions.
 *
 * Params  : 1. The extensions structure to track instrumented exceptions in.
 */
void nr_php_extension_instrument_rescan(
    nr_php_extensions_t* extensions TSRMLS_DC);

/*
 * Purpose : Destroy an extensions structure.
 *
 * Params  : 1. The extensions structure to destroy.
 */
void nr_php_extension_instrument_destroy(nr_php_extensions_t** extensions_ptr);

#endif
