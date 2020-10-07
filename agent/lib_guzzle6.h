/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains exported functions for Guzzle 6.
 */
#ifndef LIB_GUZZLE6_HDR
#define LIB_GUZZLE6_HDR

#include "php_includes.h"

/*
 * Purpose : Performs tasks that we need performed on MINIT in the Guzzle 6
 *           instrumentation.
 */
extern void nr_guzzle6_minit(TSRMLS_D);

/*
 * Purpose : Client::__construct() wrapper for Guzzle 6.
 */
extern NR_PHP_WRAPPER_PROTOTYPE(nr_guzzle6_client_construct);

#endif /* LIB_GUZZLE4_HDR */
