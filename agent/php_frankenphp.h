/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_FRANKENPHP_H
#define PHP_FRANKENPHP_H

#include "php_includes.h"
#include "nr_txn.h"

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
 *           RINIT, with nr_php_frankenphp_gate_rshutdown(), and whether the
 *           worker script has made its first call.
 *
 *           First, on a worker thread outside a worker request, name the
 *           transaction that is still open after the phase it recorded, as
 *           frankenphp_handle_request does, without ending it.
 */
extern void nr_php_frankenphp_rshutdown(void);

/*
 * Purpose : Tell whether the worker script has called
 *           frankenphp_handle_request with a valid argument in this script
 *           run. It is set on every thread, whatever its role, so a caller
 *           checks the role itself.
 *
 * Returns : True from the first such call until RSHUTDOWN.
 */
extern bool nr_php_frankenphp_requests_started(void);

/*
 * Purpose : Mark a transaction that nr_php_txn_begin is beginning on a
 *           worker thread outside a worker request, in the bootstrap or after
 *           a request, as a background job, and drop the queue start it took
 *           from X-Request-Start, which belongs to the last request.
 *           nr_php_txn_begin calls this on FrankenPHP before it decides
 *           whether to install the auto-RUM output handler.
 *
 * Params  : 1. The transaction being begun.
 */
extern void nr_php_frankenphp_txn_starting(nrtxn_t* txn);

#endif

#endif
