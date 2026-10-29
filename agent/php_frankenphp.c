/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp.h"
#include "php_agent.h"
#include "php_error.h"
#include "php_header.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_threads.h"

#include "zend_exceptions.h"

#ifdef ZTS

/*
 * The worker request in progress on this thread. Each FrankenPHP worker thread
 * runs its own worker script, so this is per thread.
 */
typedef struct _nr_php_frankenphp_worker_request_t {
  /*
   * Set by the request handler's begin handler, even when it can't begin a
   * transaction: the request handler may still begin one with
   * newrelic_start_transaction(), and that one has to end as the request's
   * transaction too, not as the bootstrap transaction. Reset by
   * nr_php_frankenphp_handle_request_end, on entry to
   * frankenphp_handle_request and at RSHUTDOWN, so a value that a bailout
   * leaves behind ends with its worker lifetime.
   */
  bool in_progress;

  /*
   * The response's Content-Type and Content-Length, taken at the request
   * handler's end because FrankenPHP destroys the response headers before
   * frankenphp_handle_request returns. The status is read when the
   * transaction ends, because reporting an uncaught exception can still set
   * it to 500. Only nr_php_frankenphp_handle_request_end reads these. They are
   * cleared on entry, after use and at RSHUTDOWN, so values that a bailout
   * leaves behind end with their worker lifetime.
   */
  char* content_type;
  int content_length;
} nr_php_frankenphp_worker_request_t;

static nrt_thread_local nr_php_frankenphp_worker_request_t
    nr_php_frankenphp_worker_request
    = {.in_progress = false, .content_type = NULL, .content_length = -1};

static void nr_php_frankenphp_clear_response_headers(void) {
  nr_free(nr_php_frankenphp_worker_request.content_type);
  nr_php_frankenphp_worker_request.content_length = -1;
}

/*
 * The begin and end handlers run on every call to the request handler. Only a
 * call from frankenphp_handle_request is a worker request: a call from user
 * code must neither begin a transaction nor record an exception that its
 * caller may still catch.
 */
static bool nr_php_frankenphp_called_from_handle_request(
    const zend_execute_data* execute_data) {
  const zend_execute_data* caller = execute_data->prev_execute_data;

  return (NULL != caller) && (NULL != caller->func)
         && (ZEND_INTERNAL_FUNCTION == caller->func->type)
         && (NULL == caller->func->common.scope)
         && nr_streq(nr_php_function_name(caller->func),
                     "frankenphp_handle_request");
}

/*
 * Records an exception that leaves the request handler, the way
 * newrelic_exception_handler does in classic mode. It has to happen here:
 * FrankenPHP reports the exception with zend_exception_error, which clears
 * EG(exception) first. nr_php_should_record_error skips the "Uncaught ..."
 * E_ERROR that FrankenPHP raises then, so the exception is recorded once.
 *
 * The exception is still pending here, and zend_call_function calls nothing
 * while one is, so the exception is set aside while it's recorded, as
 * zend_objects_destroy_object does around a destructor, for the error group
 * callback to run. An exception that the callback throws is discarded, so
 * that FrankenPHP still reports the request handler's.
 */
static void nr_php_frankenphp_record_uncaught_exception(
    zend_execute_data* execute_data,
    zend_object* exception) {
  const zend_op* opline = execute_data->opline;
  const zend_op* opline_before_exception = EG(opline_before_exception);
  zval exception_zv;

  ZVAL_OBJ(&exception_zv, exception);

  execute_data->opline = opline_before_exception;
  EG(exception) = NULL;

  nr_php_error_record_exception(
      NRPRG(txn), &exception_zv, NR_PHP_ERROR_PRIORITY_UNCAUGHT_EXCEPTION, true,
      "Uncaught exception ", &NRPRG_SHARED(exception_filters));

  if (NULL != EG(exception)) {
    zend_object* thrown = EG(exception);

    nrl_verbosedebug(NRL_INSTRUMENT,
                     "discarding an exception thrown while recording the "
                     "frankenphp request handler's uncaught exception");
    EG(exception) = NULL;
    OBJ_RELEASE(thrown);
  }

  execute_data->opline = opline;
  EG(opline_before_exception) = opline_before_exception;
  EG(exception) = exception;
}

static void nr_php_frankenphp_request_handler_fcall_begin(
    zend_execute_data* execute_data) {
  if (!nr_php_frankenphp_called_from_handle_request(execute_data)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp request handler called from user code, not "
                     "beginning a transaction");
    return;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_begin");
  // start web transaction at the start of web request handling
  nr_php_txn_begin(0, 0);
  nr_php_frankenphp_worker_request.in_progress = true;
}

static void nr_php_frankenphp_request_handler_fcall_end(
    zend_execute_data* execute_data,
    zval* return_value NRUNUSED) {
  zend_object* exception = EG(exception);

  if (!nr_php_frankenphp_called_from_handle_request(execute_data)) {
    return;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_end");

  // The transaction is ended by nr_php_frankenphp_handle_request_end.
  nr_php_frankenphp_clear_response_headers();
  nr_php_frankenphp_worker_request.content_type
      = nr_php_get_response_content_type();
  nr_php_frankenphp_worker_request.content_length
      = nr_php_get_response_content_length();

  // exit() and a graceful exit leave the request handler as exceptions too,
  // but aren't errors.
  if ((NULL != exception) && !zend_is_unwind_exit(exception)
      && !zend_is_graceful_exit(exception)) {
    nr_php_frankenphp_record_uncaught_exception(execute_data, exception);
  }
}

void nr_php_frankenphp_handle_request(INTERNAL_FUNCTION_PARAMETERS) {
  zval* function = NULL;
  zend_fcall_info_cache fcc = {0};
  zend_function* zf = NULL;
  nruserfn_t* wr = NULL;

  // nr_php_frankenphp_rshutdown resets this state after a request handler
  // bails out. Reset it here too, so that this call never acts on what an
  // earlier one left behind.
  nr_php_frankenphp_worker_request.in_progress = false;
  nr_php_frankenphp_clear_response_headers();

  // If recording, end current transaction and record it as FrankenPHP worker
  // bootstrap background transaction. If the agent is not recording it can
  // mean three things:
  // - No transaction was started because either the agent is not connected to
  //   the daemon yet, or the daemon has not verified appinfo yet.
  // - The transaction was ended by nr_php_frankenphp_handle_request_end
  // - The worker script ignored the bootstrap transaction, or ended it early
  //   with newrelic_end_of_transaction(). End it, or the request handler's
  //   begin handler would replace it without ending it, and it would leak.
  //   As in classic mode, an ignored transaction is discarded and an ended
  //   one is sent, with the name it had when it ended.
  // On later calls, a transaction that is still open was started by the worker
  // script between requests, and it is ended the same way.
  if (nr_php_recording()) {
    nr_txn_set_path("FrankenPHP worker", NRPRG(txn), "frankenphp/worker",
                    NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
    nr_txn_set_as_background_job(NRPRG(txn), "frankenphp worker");
    nr_php_txn_end(0, 0);
  } else if (NULL != NRPRG(txn)) {
    nr_php_txn_end(0, 0);
  }

  // Wrap frankenphp_handle_request's callback, which handles web request,
  // using custom fcall_begin and fcall_end handlers. fcall_begin handler
  // will start a new web transaction and nr_php_frankenphp_handle_request_end
  // will end it.
  zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "z",
                           &function);
  if (NULL != function
      && zend_is_callable_ex(function, NULL, 0, NULL, &fcc, NULL)) {
    zf = fcc.function_handler;
  }

  // A callable that resolves to __call or __callStatic gives a trampoline: a
  // temporary function that the agent can't instrument, so don't wrap it.
  if (NULL != zf && (zf->common.fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE)) {
    if (nrl_should_print(NRL_VERBOSEDEBUG, NRL_INSTRUMENT)) {
      char* name = nr_php_function_debug_name(zf);
      nrl_verbosedebug(NRL_INSTRUMENT, "%s - %s is a trampoline function",
                       __func__, NRSAFESTR(name));
      nr_free(name);
    }
  } else {
    wr = nr_php_wrap_callable(zf, NULL);
  }
  // Free the trampoline, if zend_is_callable_ex allocated one. Nothing keeps
  // zf once nr_php_wrap_callable returns.
  zend_release_fcall_info_cache(&fcc);

  if (NULL == wr) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Failed to create wraprec for frankenphp request handler");
    return;
  }
  wr->fcall_handlers.begin = nr_php_frankenphp_request_handler_fcall_begin;
  wr->fcall_handlers.end = nr_php_frankenphp_request_handler_fcall_end;
}

void nr_php_frankenphp_handle_request_end(void) {
  // In classic mode frankenphp_handle_request throws before calling the
  // request handler, and on the shutdown path it returns false without
  // calling it.
  if (!nr_php_frankenphp_worker_request.in_progress) {
    return;
  }
  nr_php_frankenphp_worker_request.in_progress = false;

  // FrankenPHP has reported any uncaught exception and flushed the output by
  // now. SG(sapi_headers).http_response_code holds the final status, and the
  // superglobals keep the request's values until the next request starts.
  nrl_verbosedebug(NRL_INSTRUMENT,
                   "frankenphp_handle_request returned, ending worker request "
                   "transaction");
  nr_php_txn_end_with_response_headers(
      nr_php_frankenphp_worker_request.content_type,
      nr_php_frankenphp_worker_request.content_length);
  nr_php_frankenphp_clear_response_headers();
}

void nr_php_frankenphp_rshutdown(void) {
  // A request handler that bailed out (exit() or a fatal error) ended the
  // worker script without reaching nr_php_frankenphp_handle_request_end, and
  // the agent's shutdown path ends its transaction.
  nr_php_frankenphp_worker_request.in_progress = false;
  nr_php_frankenphp_clear_response_headers();
}
#endif /* ZTS */
