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
  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_begin");
  // start web transaction at the start of web request handling
  nr_php_txn_begin(0, 0);
}

static void nr_php_frankenphp_request_handler_fcall_end(
    zend_execute_data* execute_data NRUNUSED,
    zval* return_value NRUNUSED) {
  // end web transaction at the end of web request handling
  nr_php_txn_end(0, 0);
  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_end");
}

void nr_php_frankenphp_handle_request(INTERNAL_FUNCTION_PARAMETERS) {
  zval* function = NULL;
  zend_function* zf;
  nruserfn_t* wr = NULL;

  // If recording, end current transaction and record it as FrankenPHP worker
  // bootstrap background transaction. If the agent is not recording it can
  // mean two things:
  // - No transaction was started because either the agent is not connected to
  //   the daemon yet, or the daemon has not verified appinfo yet.
  // - The transaction was ended by nr_php_frankenphp_request_handler_fcall_end
  if (nr_php_recording()) {
    nr_txn_set_path("FrankenPHP worker", NRPRG(txn), "frankenphp/worker",
                    NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
    nr_txn_set_as_background_job(NRPRG(txn), "frankenphp worker");
    nr_php_txn_end(0, 0);
  }

  // Wrap frankenphp_handle_request's callback, which handles web request,
  // using custom fcall_begin and fcall_end handlers. fcall_begin handler
  // will start a new web transaction and fcall_end handler will end it.
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
