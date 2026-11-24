/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp.h"
#include "php_agent.h"
#include "php_error.h"
#include "php_frankenphp_gate.h"
#include "php_globals.h"
#include "php_header.h"
#include "php_observer.h"
#include "php_user_instrument.h"
#include "php_wrapper.h"
#include "util_memory.h"
#include "util_metrics.h"
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
   * Set by the request handler's begin handler, even when it can't or may not
   * begin a transaction. When it can't, the request handler may still begin
   * one with newrelic_start_transaction(), and that one has to end as the
   * request's transaction too, not as the bootstrap transaction. When the
   * thread's mode doesn't allow one, nr_php_txn_begin refuses while this is
   * set. Reset by nr_php_frankenphp_handle_request_end, on entry to
   * frankenphp_handle_request and at RSHUTDOWN, so a value that a bailout
   * leaves behind ends with its worker lifetime.
   */
  bool in_progress;

  /*
   * Whether the worker script has called frankenphp_handle_request with a
   * valid argument in this script run, so that a transaction begun outside a
   * worker request belongs after a request and not to the bootstrap. Set by
   * the wrapper at every such call, whatever the thread's role and whether
   * or not a transaction was open, and by the confirmation of a worker
   * thread. A thread that RINIT took for classic mode needs it too: when the
   * request handler is one that the begin handler never sees, only the return
   * of the first call confirms the thread, and by then the first request has
   * run. On a worker thread under off the wrapper passes the call through
   * before it checks the argument, and doesn't set it: nothing reads it there,
   * because no transaction begins. Every reader checks the thread's role
   * itself. It lasts for the worker lifetime, so only RSHUTDOWN resets it.
   */
  bool requests_started;

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

  /*
   * When frankenphp_handle_request was called on a thread that RINIT took for
   * classic mode, relative to the start of the transaction that was open and
   * recording then, or 0. FrankenPHP waits for a request before it calls the
   * request handler, and on a quiet thread that can take hours. When the
   * begin handler shows that the thread is a worker thread, it sends that
   * transaction as the bootstrap, stopped at this time instead of after the
   * wait, unless it discards it, as nr_php_frankenphp_confirm_after_rinit
   * says. A transaction that isn't recording gets none: one that
   * newrelic_end_of_transaction() ended has its metrics, apdex and trace
   * computed already, and moving its stop time would make its event and
   * trace disagree with them. The begin handler discards it then, unless it
   * is a background job, which is sent as it ended. The wrapper clears it on
   * entry, so a call never reads what an earlier one left. RSHUTDOWN clears
   * it too.
   */
  nrtime_t canary_stop_time;

  /*
   * The absolute start time of the transaction that canary_stop_time is
   * relative to. User code can replace that transaction before the begin
   * handler runs, while FrankenPHP shuts the bootstrap's request down, as a
   * session save handler that calls newrelic_set_appname() can. The stop time
   * means nothing for the replacement, so it is applied only to a transaction
   * that started then. Set and cleared with canary_stop_time.
   */
  nrtime_t canary_txn_start;
} nr_php_frankenphp_worker_request_t;

static nrt_thread_local nr_php_frankenphp_worker_request_t
    nr_php_frankenphp_worker_request
    = {.in_progress = false,
       .requests_started = false,
       .content_type = NULL,
       .content_length = -1,
       .canary_stop_time = 0,
       .canary_txn_start = 0};

static void nr_php_frankenphp_clear_response_headers(void) {
  nr_free(nr_php_frankenphp_worker_request.content_type);
  nr_php_frankenphp_worker_request.content_length = -1;
}

/*
 * Whether the thread is a worker thread outside a worker request: in the
 * worker bootstrap, between two requests or after the last call. With
 * requests_started set as well, it is after a request. A transaction begun
 * then records no HTTP request: before the first call it records the
 * bootstrap, and after a call has returned the client has its response.
 */
static bool nr_php_frankenphp_outside_worker_request(void) {
  return NR_PHP_FRANKENPHP_THREAD_WORKER == nr_php_frankenphp_thread_role()
         && !nr_php_frankenphp_worker_request.in_progress;
}

/* Whether the thread is a worker thread whose mode is the given one. */
static bool nr_php_frankenphp_worker_in_mode(nr_php_frankenphp_mode_t mode) {
  return NR_PHP_FRANKENPHP_THREAD_WORKER == nr_php_frankenphp_thread_role()
         && mode == nr_php_frankenphp_thread_mode();
}

/*
 * Names the open transaction after the phase of the worker script it
 * recorded, if it is still recording, and makes it a background job. It is
 * one already when it began on a worker thread outside a worker request, but
 * not when it began while RINIT took the thread for classic mode. Only one
 * that isn't a background job yet is made one: once the name is frozen the
 * change is refused, and the refusal adds a supportability metric.
 *
 * An unnamed transaction's default path says nothing about what the worker
 * script did, so it gets the bootstrap name before the first call, and the
 * between-requests name after it. One that the script or a framework named
 * keeps its name. The transaction isn't ended.
 */
static void nr_php_frankenphp_name_open_txn(bool requests_started) {
  if (!nr_php_recording()) {
    return;
  }

  if (!NRTXN(status.background)) {
    nr_txn_set_as_background_job(NRPRG(txn),
                                 "FrankenPHP worker, outside a worker request");
  }

  if (NRTXN(status.path_type) < NR_PATH_TYPE_ACTION) {
    nr_txn_set_path("FrankenPHP worker, outside a worker request", NRPRG(txn),
                    requests_started
                        ? NR_PHP_FRANKENPHP_BETWEEN_REQUESTS_TXN_NAME
                        : NR_PHP_FRANKENPHP_BOOTSTRAP_TXN_NAME,
                    NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
  }
}

/*
 * Ends the open transaction, named as nr_php_frankenphp_name_open_txn names
 * it. On the first call the superglobals still hold the bootstrap's data. On
 * a later call they hold the last worker request's, and the transaction is
 * that request's after-response work, so it takes that request's URI and
 * parameters at its end.
 */
static void nr_php_frankenphp_end_open_txn(bool requests_started) {
  nr_php_frankenphp_name_open_txn(requests_started);
  nr_php_txn_end(0, 0);
}

/*
 * Ends the open transaction without the request data that its shutdown reads
 * from the superglobals: the request URI and the request parameters. This
 * runs in the middle of the worker script, once FrankenPHP has set up a
 * worker request, so the superglobals hold that request's data, which isn't
 * the transaction's. Telling nr_php_txn_end that it runs in post-deactivate
 * skips that shutdown and nothing else. This doesn't run in post-deactivate,
 * so the flag would mislead where it is passed; this name says why it is.
 */
static nr_status_t nr_php_frankenphp_txn_end_without_request_data(
    int ignoretxn) {
  return nr_php_txn_end(ignoretxn, 1);
}

/*
 * Makes a thread that RINIT took for classic mode a worker thread, once a call
 * has shown that it is one, and ends the transaction that is open.
 *
 * The request handler's begin handler can show it before the first worker
 * request runs. Then the open transaction recorded only the worker bootstrap:
 * it is the one RINIT began, or one that the worker script began in its
 * place. It began while the thread was taken for classic mode, so it wasn't
 * marked as a background job when it began. While its name isn't frozen, it
 * is sent as the bootstrap, named as nr_php_frankenphp_name_open_txn names
 * it, which also makes it a background job. One that the worker script made a
 * background job itself is sent too, and keeps its name if that is frozen. It
 * is stopped at stop_time, when frankenphp_handle_request was called, together
 * with the segments that are still open, such as the frame that runs the
 * worker loop, and not after FrankenPHP's wait for a request. stop_time is
 * relative to the transaction that started at txn_start, so it isn't applied
 * to another one. FrankenPHP has set up the first worker request by then, so
 * the transaction is ended without the request data.
 *
 * A web transaction whose name is frozen is discarded instead: the worker
 * script ended it early with newrelic_end_of_transaction(), or the browser
 * agent's footer or a CAT response header froze its name while it was still
 * recording. Its type can't change once its name is frozen, so sent as the
 * bootstrap it would be a web transaction with the first worker request's HTTP
 * status and response headers, which is wrong data. One that the worker script
 * ignored is discarded, as in classic mode. Under off it is discarded too, and
 * the thread records nothing from then on.
 *
 * Otherwise only the return of the first call shows it, and send_bootstrap is
 * false. Whatever transaction is open is discarded then, not sent and not
 * renamed. When the call reached a request handler that the begin handler
 * never sees, it is the one RINIT began, or one that the request handler
 * began in its place during the call, with newrelic_set_appname() for
 * example. FrankenPHP had set up the first worker request before it called
 * the request handler, so that transaction has recorded the request's
 * segments, errors and duration, and the wait for it: sent as the bootstrap,
 * it would be wrong data. When the worker stopped before its first request,
 * the call returned false without calling the request handler, and the RINIT
 * transaction holds the bootstrap and the wait; it is discarded too. A
 * discarded transaction doesn't go through the shutdown that reads the
 * superglobals, and sends nothing.
 *
 * A discarded transaction doesn't consume the pending metrics, so the thread's
 * next transaction carries them, and it leaves the thread's Composer package
 * scan for that transaction too. Either way the first call has been made, so
 * the thread is after a request from now on.
 */
static void nr_php_frankenphp_confirm_after_rinit(bool send_bootstrap,
                                                  nrtime_t txn_start,
                                                  nrtime_t stop_time) {
  // This decides the mode, which says whether the transaction is sent.
  nr_php_frankenphp_confirm_worker_thread();

  // A web transaction whose name is frozen can't become a background job, so
  // it is discarded rather than sent as the bootstrap.
  if (!send_bootstrap
      || NR_PHP_FRANKENPHP_MODE_OFF == nr_php_frankenphp_thread_mode()
      || (NULL != NRPRG(txn) && NRTXN(status.path_is_frozen)
          && !NRTXN(status.background))) {
    if (NULL != NRPRG(txn)) {
      nrl_verbosedebug(NRL_INSTRUMENT,
                       "frankenphp worker thread that was taken for classic "
                       "mode, discarding its transaction");
      // Ending an ignored transaction also discards the Composer package scan
      // that the thread's entry holds and that no transaction has sent. The
      // bootstrap took that scan when it loaded the autoloader, and a worker
      // thread doesn't scan again, so it belongs to the next transaction that
      // the thread sends, which fetches the entry again when it begins. So
      // this transaction lets go of the entry first. Only a discard may do
      // that: a transaction that is sent pulls the scan from the entry.
      NRTXN(composer_info.entry) = NULL;
      nr_php_txn_end(1, 0);
    }
  } else if (NULL != NRPRG(txn)) {
    // nr_txn_set_timing moves only the root's stop time, and nothing keeps
    // the other segments within it, so the open ones are stopped first.
    if (0 != stop_time && txn_start == NRTXN(abs_start_time)
        && !NRTXN(status.complete)) {
      nr_txn_set_open_segments_stop_time(NRPRG(txn), stop_time);
      nr_txn_set_timing(NRPRG(txn), NRTXN(abs_start_time), stop_time);
    }

    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp worker thread that was taken for classic "
                     "mode, ending its transaction as the bootstrap");
    nr_php_frankenphp_name_open_txn(false);
    nr_php_frankenphp_txn_end_without_request_data(0);
  }

  nr_php_frankenphp_worker_request.requests_started = true;
}

/*
 * The begin and end handlers run on every call to the request handler, in
 * place of the default ones. Only a call from frankenphp_handle_request is a
 * worker request. A call from user code gets the default handlers, so it is
 * recorded like any other function the agent wraps: it gets its own segment,
 * and it begins no transaction. An exception that leaves it, which its caller
 * may still catch, is recorded on that segment only.
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
 *
 * Expects execute_data to be the request handler's frame, as its Observer
 * end handler gets it, and exception to be the non-NULL EG(exception) that
 * leaves it, other than an exit() or a graceful exit. NRPRG(txn) may be
 * NULL: nr_php_error_record_exception checks it, as it does for
 * newrelic_exception_handler.
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
                     "frankenphp request handler called from user code, "
                     "recording it as a function call");
    nr_php_observer_fcall_begin(execute_data);
    return;
  }

  // In classic mode frankenphp_handle_request throws before it calls the
  // request handler, so a call from it shows that a thread that RINIT took
  // for classic mode is a worker thread. Confirm it before this first worker
  // request runs: the open transaction is sent as the bootstrap, stopped when
  // frankenphp_handle_request was called, or discarded, as
  // nr_php_frankenphp_confirm_after_rinit says, and the request gets its own
  // transaction below. The confirmation comes before the worker request is
  // marked in progress: the transaction that it ends recorded the bootstrap,
  // not the request.
  if (NR_PHP_FRANKENPHP_THREAD_CLASSIC == nr_php_frankenphp_thread_role()) {
    nr_php_frankenphp_confirm_after_rinit(
        true, nr_php_frankenphp_worker_request.canary_txn_start,
        nr_php_frankenphp_worker_request.canary_stop_time);
  }

  // Only a worker thread begins a worker request transaction. On a thread
  // whose run RINIT didn't classify, the open transaction is left alone.
  if (NR_PHP_FRANKENPHP_THREAD_WORKER != nr_php_frankenphp_thread_role()) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp request handler called on a thread that "
                     "isn't known to be a worker thread, not beginning a "
                     "transaction");
    return;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "frankenphp_request_handler_fcall_begin");
  // The worker request is in progress before its transaction begins, or
  // nr_php_frankenphp_txn_starting would take the transaction for one begun
  // outside a worker request, and make it a background job without its queue
  // start. It is in progress whatever the mode, so that when the mode isn't
  // instrumented, nr_php_txn_begin refuses the transactions the request
  // handler would begin itself.
  nr_php_frankenphp_worker_request.in_progress = true;

  if (NR_PHP_FRANKENPHP_MODE_INSTRUMENTED != nr_php_frankenphp_thread_mode()) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp request handler called on a worker thread "
                     "whose worker mode isn't instrumented, not beginning a "
                     "transaction");
    return;
  }

  // start web transaction at the start of web request handling
  nr_php_txn_begin(0, 0);
}

static void nr_php_frankenphp_request_handler_fcall_end(
    zend_execute_data* execute_data,
    zval* return_value) {
  zend_object* exception = EG(exception);

  if (!nr_php_frankenphp_called_from_handle_request(execute_data)) {
    nr_php_observer_fcall_end(execute_data, return_value);
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
  nr_php_frankenphp_worker_request.canary_stop_time = 0;
  nr_php_frankenphp_worker_request.canary_txn_start = 0;

  // Under off, pass the call straight through. No transaction is open on a
  // worker thread then, nor may one begin, so there is nothing to end and no
  // request handler to wrap, and the call needs no callable. Return before
  // the argument is resolved, so that no trampoline is allocated: this is the
  // one path that may return without going through the end label.
  if (nr_php_frankenphp_worker_in_mode(NR_PHP_FRANKENPHP_MODE_OFF)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp_handle_request called on a worker thread "
                     "under " NR_PHP_FRANKENPHP_INI_WORKER_MODE
                     " off, passing it through");
    return;
  }

  // Resolve the argument before touching the open transaction. The real
  // function rejects an argument that isn't callable with a TypeError or an
  // ArgumentCountError, and a call that is going to throw doesn't end the
  // bootstrap: the script goes on after the exception, and what it records
  // belongs to the transaction that is open.
  zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, ZEND_NUM_ARGS(), "z",
                           &function);
  if (NULL != function
      && zend_is_callable_ex(function, NULL, 0, NULL, &fcc, NULL)) {
    zf = fcc.function_handler;
  }

  // Once zend_is_callable_ex has run, every path leaves through the end
  // label, which releases fcc. For a callable that resolves to __call or
  // __callStatic, fcc holds a trampoline, and a path that skips the release
  // leaks one per call for the worker's lifetime. The only early return, for
  // off, comes before the argument is resolved, while fcc is still empty.
  if (NULL == zf) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "frankenphp_handle_request called without a callable, "
                     "leaving the transaction alone");
    goto end;
  }

  // On a thread that RINIT took for classic mode, the request handler's begin
  // handler may show that this is a worker thread, and send the open
  // transaction as the bootstrap, unless its name is frozen while it is a web
  // transaction. Note when it should stop: now, before FrankenPHP waits for a
  // request. A transaction that isn't recording gets no stop time:
  // newrelic_end_of_transaction() stopped it already, and it is going to be
  // discarded unless it is a background job, or it is ignored and is going to
  // be discarded.
  if (NR_PHP_FRANKENPHP_THREAD_CLASSIC == nr_php_frankenphp_thread_role()
      && nr_php_recording()) {
    nr_php_frankenphp_worker_request.canary_stop_time
        = nr_txn_now_rel(NRPRG(txn));
    nr_php_frankenphp_worker_request.canary_txn_start = NRTXN(abs_start_time);
  }

  // On a worker thread, end the transaction that is open, or the request
  // handler's begin handler would replace it without ending it, and it would
  // leak. On the first call it records the worker bootstrap: RINIT began and
  // named it, or the worker script began one in its place. On later calls the
  // worker script began it after a request. An unnamed one is named after
  // that phase. As in classic mode, an ignored transaction is discarded, and
  // one ended early with newrelic_end_of_transaction() is sent with the name
  // it had when it ended. None is open when the agent wasn't connected to the
  // daemon yet at RINIT, when nr_php_frankenphp_handle_request_end has ended
  // the last request's, or when the confirmation of a thread that RINIT took
  // for classic mode, by the begin handler or by the return of the first
  // call, ended the one that was open. On an unsupported version this ends
  // the bootstrap, and the transactions begun between requests, the same way.
  //
  // On any other thread, leave the transaction alone. In classic mode the
  // real function throws before it calls the request handler, and the rest of
  // the request is recorded as usual.
  if (NR_PHP_FRANKENPHP_THREAD_WORKER == nr_php_frankenphp_thread_role()
      && NULL != NRPRG(txn)) {
    nr_php_frankenphp_end_open_txn(
        nr_php_frankenphp_worker_request.requests_started);
  }

  // From this call on, a transaction begun outside a worker request is begun
  // after a request, also when no transaction was open to end here, as on a
  // cold start. It is recorded on every thread: on one that RINIT took for
  // classic mode, the return of this call may be what shows it is a worker
  // thread, after its first request has run, or when the worker stopped before
  // its first request and the call returned false without calling the request
  // handler. In classic mode the call throws, and nothing reads it.
  nr_php_frankenphp_worker_request.requests_started = true;

  // Wrap frankenphp_handle_request's callback, which handles web request,
  // using custom fcall_begin and fcall_end handlers. On a worker thread the
  // fcall_begin handler starts a new web transaction and
  // nr_php_frankenphp_handle_request_end ends it. The callback is wrapped on
  // any thread: Zend installs a function's Observer handlers at its first
  // call, so a thread that turns out to be a worker thread only gets the
  // custom handlers on later calls if they were set before the first one. It
  // is wrapped on an unsupported version too: the fcall_begin handler begins
  // no transaction then, but marks the worker request as in progress, during
  // which nr_php_txn_begin refuses.
  //
  // A trampoline is a temporary function that the agent can't instrument, so
  // don't wrap it.
  if (zf->common.fn_flags & ZEND_ACC_CALL_VIA_TRAMPOLINE) {
    if (nrl_should_print(NRL_VERBOSEDEBUG, NRL_INSTRUMENT)) {
      char* name = nr_php_function_debug_name(zf);
      nrl_verbosedebug(NRL_INSTRUMENT, "%s - %s is a trampoline function",
                       __func__, NRSAFESTR(name));
      nr_free(name);
    }
    goto end;
  }

  wr = nr_php_wrap_callable(zf, NULL);
  if (NULL == wr) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Failed to create wraprec for frankenphp request handler");
    goto end;
  }
  wr->fcall_handlers.begin = nr_php_frankenphp_request_handler_fcall_begin;
  wr->fcall_handlers.end = nr_php_frankenphp_request_handler_fcall_end;

end:
  // Free the trampoline, if zend_is_callable_ex allocated one. Nothing keeps
  // zf once nr_php_wrap_callable returns.
  zend_release_fcall_info_cache(&fcc);
}

void nr_php_frankenphp_handle_request_end(void) {
  // Classic mode throws before calling the request handler, and an invalid
  // argument leaves a TypeError pending. So a call that returns with no
  // exception pending shows that a thread that RINIT took for classic mode
  // is a worker thread. The begin handler confirms such a thread before the
  // first request runs, so it is still taken for classic mode here only when
  // the call didn't reach a request handler that the begin handler sees: the
  // request handler is a trampoline, an internal function, or a closure
  // called before it was passed in, or the worker stopped before its first
  // request, and the call returned false without calling it. Confirm it now,
  // so that its next call is a worker request, and its RINIT transaction
  // isn't left open for the worker lifetime. That transaction holds the first
  // request, or the bootstrap and the wait for a request that never came, so
  // it is discarded.
  if (NR_PHP_FRANKENPHP_THREAD_CLASSIC == nr_php_frankenphp_thread_role()
      && NULL == EG(exception)) {
    nr_php_frankenphp_confirm_after_rinit(false, 0, 0);
  }

  // The begin handler began no worker request transaction: the thread isn't
  // a worker thread, or wasn't one yet when the call began, the request
  // handler is one the agent couldn't wrap, or the call didn't reach it. In
  // classic mode frankenphp_handle_request throws before calling the request
  // handler, and on the shutdown path it returns false without calling it.
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
  // A transaction still open on a worker thread outside a worker request is
  // ended in post-deactivate. Name it now, while the thread's role and
  // in_progress still say which phase it recorded: a boot that fails before
  // the first call, after the worker script replaced the bootstrap
  // transaction, is sent as the bootstrap, and one that the worker script
  // starts after its last call as a between-requests one. After a normal exit
  // from the worker loop nothing is open, because the last call ended it.
  // The shutdown that attached the request data has run already, and it
  // doesn't freeze the name.
  if (nr_php_frankenphp_outside_worker_request()) {
    nr_php_frankenphp_name_open_txn(
        nr_php_frankenphp_worker_request.requests_started);
  }

  // A request handler that bailed out (exit() or a fatal error) ended the
  // worker script without reaching nr_php_frankenphp_handle_request_end, and
  // the agent's shutdown path ends its transaction.
  nr_php_frankenphp_worker_request.in_progress = false;
  nr_php_frankenphp_clear_response_headers();
  nr_php_frankenphp_worker_request.canary_stop_time = 0;
  nr_php_frankenphp_worker_request.canary_txn_start = 0;

  // The next script run on this thread starts a new worker lifetime, or is
  // classic mode.
  nr_php_frankenphp_worker_request.requests_started = false;

  // What RINIT learned is for this script run only.
  nr_php_frankenphp_gate_rshutdown();
}

bool nr_php_frankenphp_requests_started(void) {
  return nr_php_frankenphp_worker_request.requests_started;
}

void nr_php_frankenphp_txn_starting(nrtxn_t* txn) {
  if (NULL == txn || !nr_php_frankenphp_outside_worker_request()) {
    return;
  }

  // It has to be a background job before nr_php_txn_begin decides whether to
  // install the auto-RUM output handler, which doesn't check again later.
  nr_txn_set_as_background_job(txn,
                               "FrankenPHP worker, outside a worker request");

  // The transaction keeps the rest of the data that the superglobals hold.
  // After a request they still hold that request's, as FrankenPHP documents,
  // and so is X-Request-Start: a queue time would measure the request, not
  // this transaction. Being a background job drops the queue metric, but the
  // browser agent's footer and the CAT response header read the queue start
  // whatever the transaction's type.
  txn->status.http_x_start = 0;
}

bool nr_php_frankenphp_txn_begin_refused(void) {
  // A thread that isn't a worker thread pays this one read.
  if (NR_PHP_FRANKENPHP_THREAD_WORKER != nr_php_frankenphp_thread_role()) {
    return false;
  }

  switch (nr_php_frankenphp_thread_mode()) {
    case NR_PHP_FRANKENPHP_MODE_OFF:
      return true;
    case NR_PHP_FRANKENPHP_MODE_UNSUPPORTED:
      return nr_php_frankenphp_worker_request.in_progress;
    case NR_PHP_FRANKENPHP_MODE_INSTRUMENTED:
      break;
  }

  return false;
}

void nr_php_frankenphp_txn_begun(nrtxn_t* txn) {
  nr_php_frankenphp_version_t version = {0, 0, 0};
  char* name;

  if (NULL == txn || !nr_php_frankenphp_worker_request.in_progress
      || !nr_php_frankenphp_worker_in_mode(
          NR_PHP_FRANKENPHP_MODE_INSTRUMENTED)) {
    return;
  }

  if (nr_php_frankenphp_parse_version(
          NR_PHP_PROCESS_GLOBALS(frankenphp_version), &version)) {
    name = nr_formatf(NR_PHP_FRANKENPHP_METRIC_WORKER_MODE "%d.%d.%d",
                      version.major, version.minor, version.patch);
  } else {
    name = nr_strdup(NR_PHP_FRANKENPHP_METRIC_WORKER_MODE "unknown");
  }

  nrm_force_add(txn->unscoped_metrics, name, 0);
  nr_free(name);
}
#endif /* ZTS */
