/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_FRANKENPHP_H
#define PHP_FRANKENPHP_H

#include "php_includes.h"

#ifdef ZTS

extern void nr_php_frankenphp_handle_request(INTERNAL_FUNCTION_PARAMETERS);

/*
 * Purpose : End the worker request transaction once frankenphp_handle_request
 *           has returned, if this call began one.
 */
extern void nr_php_frankenphp_handle_request_end(void);

/*
 * Purpose : Free the response headers saved for a worker request transaction
 *           that a bailout left unended.
 */
extern void nr_php_frankenphp_rshutdown(void);

#endif

#endif
