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
 *           has returned, if this call began one. A call that returns with no
 *           exception pending on a thread that RINIT took for classic mode
 *           makes it a worker thread, and discards its open transaction.
 */
extern void nr_php_frankenphp_handle_request_end(void);

/*
 * Purpose : Reset the worker request state that a bailout left behind: the
 *           flag that a transaction was begun, and the saved response
 *           headers. Also reset what the gate learned about the thread at
 *           RINIT, with nr_php_frankenphp_gate_rshutdown().
 */
extern void nr_php_frankenphp_rshutdown(void);

#endif

#endif
