/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains exported functions for Guzzle 4.
 */
#ifndef LIB_GUZZLE4_HDR
#define LIB_GUZZLE4_HDR

#include "php_includes.h"

/*
 * Purpose : Performs tasks that we need performed on MINIT in the Guzzle 4
 *           instrumentation.
 */
extern void nr_guzzle4_minit(TSRMLS_D);

/*
 * Purpose : Performs tasks that we need performed on RSHUTDOWN in the Guzzle 4
 *           instrumentation.
 */
extern void nr_guzzle4_rshutdown(TSRMLS_D);

/*
 * Purpose : Client::__construct() wrapper for Guzzle 4.
 */
extern NR_PHP_WRAPPER_PROTOTYPE(nr_guzzle4_client_construct);

#endif /* LIB_GUZZLE4_HDR */
