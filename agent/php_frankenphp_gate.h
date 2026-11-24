/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * The gate decides whether the agent instruments FrankenPHP's worker mode. It
 * keeps what it learns about the FrankenPHP process, which the agent reads at
 * every MINIT, and the helpers that read it.
 */

#ifndef PHP_FRANKENPHP_GATE_H
#define PHP_FRANKENPHP_GATE_H

#include "php_includes.h"
#include "nr_txn.h"

#ifdef ZTS

/*
 * The names that are still open. Each is defined here and nowhere else, and
 * the code and the tests use these macros, so renaming one means changing only
 * this header. The INI setting is the exception:
 * agent/scripts/newrelic.ini.template and some comments spell its name out too.
 */

/*
 * TODO-NAME: the INI setting that turns worker mode instrumentation on or
 * off.
 */
#define NR_PHP_FRANKENPHP_INI_WORKER_MODE "newrelic.frankenphp.worker_mode"

/* TODO-NAME: followed by X.Y.Z, or unknown. */
#define NR_PHP_FRANKENPHP_METRIC_WORKER_MODE \
  "Supportability/PHP/FrankenPHP/WorkerMode/Version/"

/*
 * TODO-NAME: followed by the version as read, cut to 16 bytes and sanitised,
 * or unknown when there is none.
 */
#define NR_PHP_FRANKENPHP_METRIC_UNSUPPORTED \
  "Supportability/PHP/FrankenPHP/WorkerMode/Unsupported/"

/*
 * How many bytes of the version the unsupported metric keeps. A release's
 * version fits, and so do the first 16 hex digits of the git SHA that a
 * static build without a version reports, which are enough to name the
 * commit.
 */
#define NR_PHP_FRANKENPHP_VERSION_METRIC_MAX 16

/* TODO-NAME: a worker thread that RINIT took for classic mode. */
#define NR_PHP_FRANKENPHP_METRIC_MISSED_AT_RINIT \
  "Supportability/PHP/FrankenPHP/WorkerMode/MissedAtRINIT"

/* TODO-NAME: the transaction that is open on a worker between two requests. */
#define NR_PHP_FRANKENPHP_BETWEEN_REQUESTS_TXN_NAME \
  "frankenphp/worker/between_requests"

/*
 * The name of the worker bootstrap transaction. It is sent as
 * OtherTransaction/Custom/frankenphp/worker/bootstrap.
 */
#define NR_PHP_FRANKENPHP_BOOTSTRAP_TXN_NAME "frankenphp/worker/bootstrap"

typedef struct {
  int major;
  int minor;
  int patch;
} nr_php_frankenphp_version_t;

/*
 * Purpose : Parse a FrankenPHP version, as the module's version string holds
 *           it.
 *
 *           One leading lowercase v is stripped. What is left must be exactly
 *           X.Y.Z: three decimal numbers, each starting with a digit and
 *           fitting in an int, with nothing before, between or after them.
 *           Anything else, such as dev, a git SHA, a release candidate or an
 *           empty string, is an unknown version.
 *
 * Params  : 1. The version string. May be NULL.
 *           2. Where to store the parsed version.
 *
 * Returns : True if the version was parsed. False for an unknown version, in
 *           which case the output is left alone.
 */
extern bool nr_php_frankenphp_parse_version(const char* version,
                                            nr_php_frankenphp_version_t* out);

/*
 * Purpose : Record what the agent learned about the process at MINIT, and
 *           clear the log-once flags.
 *
 *           MINIT runs again when the server reloads its configuration, so
 *           this is called again then, and what it records replaces what an
 *           earlier call recorded.
 *
 *           It runs only at MINIT, when no thread serves a request, so
 *           nothing reads the process globals while it writes them, and it
 *           writes them without synchronisation.
 *
 *           Sets is_frankenphp in the process globals to 1 when the SAPI is
 *           frankenphp, and to 0 otherwise. Replaces frankenphp_version with a
 *           copy of the version when the SAPI is frankenphp, or with NULL
 *           when it isn't or when the version is NULL.
 *
 * Params  : 1. The name of the SAPI. May be NULL.
 *           2. The version of the frankenphp module. May be NULL.
 */
extern void nr_php_frankenphp_set_process_info(const char* sapi_name,
                                               const char* version);

/*
 * The lines that nr_php_frankenphp_log_once() logs once each.
 * NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT must stay last: the flags are sized
 * from it, and nr_php_frankenphp_log_once() refuses a line past it, so a line
 * added after it would never be logged.
 */
typedef enum {
  NR_PHP_FRANKENPHP_LOG_OFF,
  NR_PHP_FRANKENPHP_LOG_WORKER_ENV_SET,
  NR_PHP_FRANKENPHP_LOG_NO_WORKER_ENV,
  NR_PHP_FRANKENPHP_LOG_UNKNOWN_VERSION,
  NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT
} nr_php_frankenphp_log_once_t;

/*
 * Purpose : Tell the caller whether it is the first to log the given line
 *           since the last nr_php_frankenphp_set_process_info().
 *
 *           It is thread safe. A misconfigured FRANKENPHP_WORKER makes every
 *           classic-mode RINIT on every thread ask, so once a line has been
 *           logged, asking again takes no lock and writes nothing.
 *
 * Params  : 1. Which line.
 *
 * Returns : True the first time it is called for the line, false after that.
 */
extern bool nr_php_frankenphp_log_once(nr_php_frankenphp_log_once_t what);

/*
 * What a script run turned out to be, going by the two signals FrankenPHP
 * gives the agent: the request URI and the FRANKENPHP_WORKER variable.
 *
 * Each signal catches a case in which the other fails, so a run is a worker
 * bootstrap only when both say so. When they disagree the run is classic mode,
 * and the two kinds of disagreement are told apart so that each can be logged.
 */
typedef enum {
  /* An HTTP request, or a run that has neither signal. */
  NR_PHP_FRANKENPHP_RUN_CLASSIC,
  /* A worker script's bootstrap: both signals say so. */
  NR_PHP_FRANKENPHP_RUN_BOOTSTRAP,
  /* The variable is "1", but the URI isn't bootstrap-shaped. */
  NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET,
  /* The URI is bootstrap-shaped, but the variable isn't "1". */
  NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV
} nr_php_frankenphp_run_t;

/*
 * Purpose : Classify a script run from the request URI and the value of the
 *           FRANKENPHP_WORKER variable.
 *
 *           The URI is bootstrap-shaped when it isn't NULL, isn't empty, has
 *           no leading slash and isn't *. The variable counts only when it is
 *           exactly "1": "0", "true", "" and " 1" count as unset.
 *
 * Params  : 1. The request URI. May be NULL.
 *           2. The value of FRANKENPHP_WORKER. May be NULL.
 *
 * Returns : How the run is classified.
 */
extern nr_php_frankenphp_run_t nr_php_frankenphp_classify_run(
    const char* request_uri,
    const char* frankenphp_worker);

/* What newrelic.frankenphp.worker_mode is set to. */
typedef enum {
  NR_PHP_FRANKENPHP_SETTING_AUTO = 0,
  NR_PHP_FRANKENPHP_SETTING_ON,
  NR_PHP_FRANKENPHP_SETTING_OFF
} nr_php_frankenphp_setting_t;

/* What the agent does with worker mode, given the setting and the version. */
typedef enum {
  /* Worker mode is recorded. */
  NR_PHP_FRANKENPHP_MODE_INSTRUMENTED,
  /* The version is one the agent doesn't support, so nothing is recorded. */
  NR_PHP_FRANKENPHP_MODE_UNSUPPORTED,
  /* The setting turned worker mode instrumentation off. */
  NR_PHP_FRANKENPHP_MODE_OFF
} nr_php_frankenphp_mode_t;

/*
 * Purpose : Decide what the agent does with worker mode.
 *
 *           With the setting off the answer is off, whatever the version. With
 *           the setting auto, worker mode is instrumented from v1.11.2, the
 *           oldest version the agent supports, and unsupported below that.
 *           With the setting on, it is instrumented from v1.2.4, the oldest
 *           version it can work with, and also when the version is unknown,
 *           as on a custom build, and unsupported only below v1.2.4. The two
 *           floors are fixed, not tied to the latest release.
 *
 * Params  : 1. The setting.
 *           2. The version as read from FrankenPHP, which may be NULL. It is
 *              parsed by nr_php_frankenphp_parse_version().
 *
 * Returns : What the agent does.
 */
extern nr_php_frankenphp_mode_t nr_php_frankenphp_mode_for(
    nr_php_frankenphp_setting_t setting,
    const char* version);

/* What the agent takes the current thread for, in this script run. */
typedef enum {
  /* RINIT didn't classify the run, as when the agent is disabled for it. */
  NR_PHP_FRANKENPHP_THREAD_UNKNOWN = 0,
  /* RINIT took the run for classic mode, and nothing has shown otherwise. */
  NR_PHP_FRANKENPHP_THREAD_CLASSIC,
  /* A worker thread: RINIT saw a worker bootstrap, or a call showed it. */
  NR_PHP_FRANKENPHP_THREAD_WORKER
} nr_php_frankenphp_thread_role_t;

/*
 * Purpose : Classify the script run, and begin its transaction. RINIT calls
 *           this on FrankenPHP instead of beginning the transaction itself.
 *
 *           First it drops the metrics that are still pending from the last
 *           script run on the thread, which no transaction consumed.
 *
 *           The run is classified once, from the request URI and the
 *           FRANKENPHP_WORKER variable, and the answer is kept until
 *           RSHUTDOWN. A worker bootstrap makes the thread a worker thread,
 *           and decides its mode from the setting and the version. Unless the
 *           mode is off, its transaction is named as the bootstrap and made a
 *           background job as soon as it begins. Every other run is classic
 *           mode, and its transaction begins as on any other SAPI. Each of
 *           the two ways in which the signals can disagree is logged once per
 *           process.
 */
extern void nr_php_frankenphp_rinit(void);

/*
 * Purpose : Tell what the agent takes the current thread for.
 *
 * Returns : The thread's role until RSHUTDOWN.
 */
extern nr_php_frankenphp_thread_role_t nr_php_frankenphp_thread_role(void);

/*
 * Purpose : Tell what the agent does with worker mode on the current thread.
 *
 * Returns : The mode that nr_php_frankenphp_mode_for() gives for the setting
 *           and the version read at MINIT, decided when the thread became a
 *           worker thread. Meaningful only on a worker thread.
 */
extern nr_php_frankenphp_mode_t nr_php_frankenphp_thread_mode(void);

/*
 * Purpose : Make the current thread a worker thread, once a call has shown
 *           that RINIT took it for classic mode wrongly, and decide its mode
 *           as RINIT does for a worker bootstrap.
 */
extern void nr_php_frankenphp_confirm_worker_thread(void);

/*
 * Purpose : Forget what RINIT learned about the current thread: its role and
 *           mode, the run's classification and the request URI.
 *
 *           The pending metrics are kept: a transaction still open when the
 *           script run ends is sent after RSHUTDOWN, and carries them.
 */
extern void nr_php_frankenphp_gate_rshutdown(void);

/*
 * The supportability metrics that wait for the next transaction the thread
 * sends, as bits. The transaction that would carry them often doesn't exist
 * when the agent learns of them, as on a cold start, so they go on whichever
 * transaction the thread sends next in the worker lifetime.
 */
typedef enum {
  /* The thread is a worker thread on a version the setting doesn't allow. */
  NR_PHP_FRANKENPHP_PENDING_UNSUPPORTED = 1,
  /* RINIT took a worker thread for classic mode. */
  NR_PHP_FRANKENPHP_PENDING_MISSED_AT_RINIT = 2
} nr_php_frankenphp_pending_t;

/*
 * Purpose : Tell which metrics are pending on the current thread.
 *
 * Returns : The nr_php_frankenphp_pending_t bits that are set.
 */
extern unsigned nr_php_frankenphp_thread_pending_metrics(void);

/*
 * Purpose : Build the name of the unsupported metric for a version.
 *
 *           The last segment is unknown when the version is NULL or empty.
 *           Otherwise it is the first NR_PHP_FRANKENPHP_VERSION_METRIC_MAX
 *           bytes of the version, with every byte other than an ASCII letter,
 *           a digit, ., _, + or - replaced by _, so that the version can't add
 *           segments to the name or make it invalid UTF-8.
 *
 * Params  : 1. The version as read from FrankenPHP. May be NULL.
 *
 * Returns : The metric name, which the caller must free.
 */
extern char* nr_php_frankenphp_unsupported_metric_name(const char* version);

/*
 * Purpose : Add the pending metrics to a transaction that is about to be
 *           sent. They stay pending until nr_php_frankenphp_txn_sent() says
 *           that the send succeeded.
 *
 *           It reads no thread state but the pending metrics, so it also
 *           works in post-deactivate, after RSHUTDOWN.
 *
 * Params  : 1. The transaction.
 */
extern void nr_php_frankenphp_txn_sending(nrtxn_t* txn);

/*
 * Purpose : Clear the pending metrics, once the transaction that carries them
 *           has been sent.
 */
extern void nr_php_frankenphp_txn_sent(void);

#endif /* ZTS */

#endif /* PHP_FRANKENPHP_GATE_H */
