/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef TEST_PHP_INCLUDES_HDR
#define TEST_PHP_INCLUDES_HDR

#ifdef __GNUC__
#pragma GCC system_header
#endif

#include <sapi/embed/php_embed.h>

/*
 * Potentially missing thread safety macros.
 */
#ifndef PTSRMLS_DC
#define PTSRMLS_D void
#define PTSRMLS_DC
#define PTSRMLS_C
#define PTSRMLS_CC
#endif /* PTSRMLS_DC */

#endif /* TEST_PHP_INCLUDES_HDR */
