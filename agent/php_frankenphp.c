/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp.h"
#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"

#ifdef ZTS

static void nr_php_frankenphp_request_handler_fcall_begin(
    zend_execute_data* execute_data NRUNUSED) {
  nrl_verbosedebug(NRL_INSTRUMENT,
                   "frankenphp_request_handler_fcall_begin started");
  // emulate rinit
  nr_php_txn_begin(0, 0);
  nrl_verbosedebug(NRL_INSTRUMENT,
                   "frankenphp_request_handler_fcall_begin done");
}

static void nr_php_frankenphp_request_handler_fcall_end(
    zend_execute_data* execute_data NRUNUSED,
    zval* return_value NRUNUSED) {
  nrl_verbosedebug(NRL_INSTRUMENT,
                   "frankenphp_request_handler_fcall_end started");
  // emulate rshutdown
  nr_php_txn_end(0, 0);
  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_end done");
}

// Wrap the request handler with before and after hooks to emulate rinit and
// rshutdown
void nr_php_frankenphp_handle_request(INTERNAL_FUNCTION_PARAMETERS) {
  zval* function = NULL;
  zend_function* zf;
  nruserfn_t* wr = NULL;

  // Always end current transaction started when worker was started
  // Maybe store package data?
  nr_php_txn_end(1, 0);

  zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "z",
                           &function);
  zf = nr_php_zval_to_function(function);

  wr = nr_php_wrap_callable(zf, NULL);
  if (NULL == wr) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Failed to create wraprec for frankenphp request handler");
    return;
  }
  wr->fcall_handlers.begin = nr_php_frankenphp_request_handler_fcall_begin;
  wr->fcall_handlers.end = nr_php_frankenphp_request_handler_fcall_end;
}
#endif /* ZTS */
