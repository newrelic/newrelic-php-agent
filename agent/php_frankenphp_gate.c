/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp_gate.h"
#include "php_agent.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_metrics.h"
#include "util_strings.h"
#include "util_threads.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef ZTS

/*
 * agent-specs limits a metric name to 255 bytes. Neither the agent nor the
 * daemon checks it, so make sure that a renamed prefix can't break it.
 */
_Static_assert(sizeof(NR_PHP_FRANKENPHP_METRIC_UNSUPPORTED) - 1
                       + NR_PHP_FRANKENPHP_VERSION_METRIC_MAX
                   <= 255,
               "the unsupported metric's name must fit in 255 bytes");

/*
 * Reads one decimal number at the cursor, and moves the cursor past it.
 *
 * strtol skips leading white space and accepts a sign, neither of which a
 * version number may have, so the number has to start with an ASCII digit.
 * Anything that doesn't fit in an int is rejected.
 */
static bool nr_php_frankenphp_parse_number(const char** cursor, int* number) {
  const char* start = *cursor;
  char* end = NULL;
  long value;

  if (*start < '0' || *start > '9') {
    return false;
  }

  errno = 0;
  value = strtol(start, &end, 10);
  if (ERANGE == errno || value > INT_MAX) {
    return false;
  }

  *cursor = end;
  *number = (int)value;
  return true;
}

bool nr_php_frankenphp_parse_version(const char* version,
                                     nr_php_frankenphp_version_t* out) {
  const char* cursor = version;
  int numbers[3];
  int i;

  if (NULL == version || NULL == out) {
    return false;
  }

  if ('v' == *cursor) {
    cursor += 1;
  }

  for (i = 0; i < 3; i++) {
    if (!nr_php_frankenphp_parse_number(&cursor, &numbers[i])) {
      return false;
    }

    if (i < 2) {
      if ('.' != *cursor) {
        return false;
      }
      cursor += 1;
    }
  }

  if ('\0' != *cursor) {
    return false;
  }

  out->major = numbers[0];
  out->minor = numbers[1];
  out->patch = numbers[2];
  return true;
}

/*
 * The log-once flags, one for each kind of line. They are process-wide and
 * read on every classic-mode RINIT on every thread, so they are synchronised
 * with the compiler's atomic built-ins and no lock.
 *
 * The built-ins are only lock free for a flag that the CPU can read and write
 * in one access. A type that needed libatomic would be a link error at best, so
 * turn it into a compile error.
 */
#define NR_PHP_FRANKENPHP_LOG_ONCE_COUNT \
  (NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT + 1)

static bool nr_php_frankenphp_log_once_flags[NR_PHP_FRANKENPHP_LOG_ONCE_COUNT];

_Static_assert(
    __atomic_always_lock_free(sizeof(nr_php_frankenphp_log_once_flags[0]), 0),
    "the log-once flags must be lock free, without libatomic");

void nr_php_frankenphp_set_process_info(const char* sapi_name,
                                        const char* version) {
  bool is_frankenphp = (0 == nr_strcmp(sapi_name, "frankenphp"));
  int i;

  NR_PHP_PROCESS_GLOBALS(is_frankenphp) = is_frankenphp ? 1 : 0;

  nr_free(NR_PHP_PROCESS_GLOBALS(frankenphp_version));
  if (is_frankenphp && NULL != version) {
    NR_PHP_PROCESS_GLOBALS(frankenphp_version) = nr_strdup(version);
  }

  for (i = 0; i < NR_PHP_FRANKENPHP_LOG_ONCE_COUNT; i++) {
    __atomic_store_n(&nr_php_frankenphp_log_once_flags[i], false,
                     __ATOMIC_RELEASE);
  }
}

bool nr_php_frankenphp_log_once(nr_php_frankenphp_log_once_t what) {
  bool* flag;

  if ((int)what < 0 || (int)what >= NR_PHP_FRANKENPHP_LOG_ONCE_COUNT) {
    return false;
  }
  flag = &nr_php_frankenphp_log_once_flags[what];

  /*
   * Once the line has been logged, this read is all that runs. It takes no
   * lock and writes nothing, so the threads don't contend for the flag.
   */
  if (__atomic_load_n(flag, __ATOMIC_ACQUIRE)) {
    return false;
  }

  /*
   * Several threads can get here together. The exchange lets exactly one of
   * them see the flag still clear.
   */
  return !__atomic_exchange_n(flag, true, __ATOMIC_ACQ_REL);
}

/*
 * A worker bootstrap's request URI is the worker script's path, as FrankenPHP
 * gives it, so it has no leading slash. An HTTP request's has one, an
 * OPTIONS * request's is *, and an embedding program or a run with no request
 * has an empty one or none. The agent takes those for classic mode, which is
 * the safe direction: it keeps today's behaviour.
 */
static bool nr_php_frankenphp_uri_is_bootstrap_shaped(const char* request_uri) {
  if (NULL == request_uri || '\0' == request_uri[0]) {
    return false;
  }

  return '/' != request_uri[0] && !nr_streq(request_uri, "*");
}

nr_php_frankenphp_run_t nr_php_frankenphp_classify_run(
    const char* request_uri,
    const char* frankenphp_worker) {
  bool bootstrap_shaped
      = nr_php_frankenphp_uri_is_bootstrap_shaped(request_uri);
  bool worker_env = nr_streq(frankenphp_worker, "1");

  if (bootstrap_shaped && worker_env) {
    return NR_PHP_FRANKENPHP_RUN_BOOTSTRAP;
  }

  if (worker_env) {
    return NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET;
  }

  if (bootstrap_shaped) {
    return NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV;
  }

  return NR_PHP_FRANKENPHP_RUN_CLASSIC;
}

/*
 * The oldest version the agent supports worker mode on, and the oldest it can
 * work with at all. They are fixed values, not tied to the latest release.
 */
static const nr_php_frankenphp_version_t nr_php_frankenphp_support_floor
    = {1, 11, 2};
static const nr_php_frankenphp_version_t nr_php_frankenphp_technical_floor
    = {1, 2, 4};

static bool nr_php_frankenphp_version_at_least(
    const nr_php_frankenphp_version_t* version,
    const nr_php_frankenphp_version_t* minimum) {
  if (version->major != minimum->major) {
    return version->major > minimum->major;
  }

  if (version->minor != minimum->minor) {
    return version->minor > minimum->minor;
  }

  return version->patch >= minimum->patch;
}

nr_php_frankenphp_mode_t nr_php_frankenphp_mode_for(
    nr_php_frankenphp_setting_t setting,
    const char* version) {
  nr_php_frankenphp_version_t parsed = {0, 0, 0};
  bool known;

  if (NR_PHP_FRANKENPHP_SETTING_OFF == setting) {
    return NR_PHP_FRANKENPHP_MODE_OFF;
  }

  known = nr_php_frankenphp_parse_version(version, &parsed);

  if (known
      && nr_php_frankenphp_version_at_least(&parsed,
                                            &nr_php_frankenphp_support_floor)) {
    return NR_PHP_FRANKENPHP_MODE_INSTRUMENTED;
  }

  if (NR_PHP_FRANKENPHP_SETTING_ON != setting) {
    return NR_PHP_FRANKENPHP_MODE_UNSUPPORTED;
  }

  /*
   * The setting is on, and the version is below the support floor or unknown.
   * An unknown version is taken to be a custom build and is allowed through.
   */
  if (!known
      || nr_php_frankenphp_version_at_least(
          &parsed, &nr_php_frankenphp_technical_floor)) {
    return NR_PHP_FRANKENPHP_MODE_INSTRUMENTED;
  }

  return NR_PHP_FRANKENPHP_MODE_UNSUPPORTED;
}

/*
 * What the agent learned about the current thread at RINIT, kept until
 * RSHUTDOWN. Each FrankenPHP thread runs its own script, so this is per
 * thread. It allocates nothing.
 */
typedef struct {
  nr_php_frankenphp_thread_role_t role;
  /*
   * Decided when the thread becomes a worker thread, and meaningful only on a
   * worker thread.
   */
  nr_php_frankenphp_mode_t mode;
  nr_php_frankenphp_run_t run;
  /*
   * The request URI seen at RINIT, truncated, and empty when there was none.
   * It is kept whatever its shape, because SG(request_info) can't be read
   * later instead: FrankenPHP updates it for each worker request.
   */
  char rinit_uri[256];
  /*
   * The nr_php_frankenphp_pending_t bits. Unlike the rest, they outlive
   * RSHUTDOWN: a transaction still open at the end of the script run is sent
   * in post-deactivate, after RSHUTDOWN, and carries them. They are dropped
   * at the next RINIT, so that what one worker lifetime didn't send can't go
   * on another lifetime's transactions, or on a classic-mode one.
   */
  unsigned pending;
} nr_php_frankenphp_thread_t;

static nrt_thread_local nr_php_frankenphp_thread_t nr_php_frankenphp_thread
    = {.role = NR_PHP_FRANKENPHP_THREAD_UNKNOWN,
       .mode = NR_PHP_FRANKENPHP_MODE_INSTRUMENTED,
       .run = NR_PHP_FRANKENPHP_RUN_CLASSIC,
       .rinit_uri = "",
       .pending = 0};

static const char* nr_php_frankenphp_setting_name(
    nr_php_frankenphp_setting_t setting) {
  switch (setting) {
    case NR_PHP_FRANKENPHP_SETTING_ON:
      return "on";
    case NR_PHP_FRANKENPHP_SETTING_OFF:
      return "off";
    case NR_PHP_FRANKENPHP_SETTING_AUTO:
      break;
  }

  return "auto";
}

/*
 * Decides the mode of a thread that has just become a worker thread, from the
 * setting and the version read at MINIT, and logs what it means.
 *
 * A version that the setting doesn't allow makes the unsupported metric
 * pending, for the next transaction the thread sends, and is logged at
 * warning once per worker lifetime, whether or not a transaction began. The
 * warning names the version as read, not as the metric cuts it, because a
 * support engineer needs the exact string. The setting off, and an unknown
 * version that the setting on lets through, are logged once per process.
 */
static void nr_php_frankenphp_decide_mode(void) {
  nr_php_frankenphp_setting_t setting
      = (nr_php_frankenphp_setting_t)NRINI(frankenphp_worker_mode);
  const char* version = NR_PHP_PROCESS_GLOBALS(frankenphp_version);
  nr_php_frankenphp_version_t parsed = {0, 0, 0};

  nr_php_frankenphp_thread.mode = nr_php_frankenphp_mode_for(setting, version);

  switch (nr_php_frankenphp_thread.mode) {
    case NR_PHP_FRANKENPHP_MODE_OFF:
      if (nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF)) {
        nrl_info(NRL_INIT, NR_PHP_FRANKENPHP_INI_WORKER_MODE
                 " is off, so the agent records nothing on FrankenPHP worker "
                 "threads");
      }
      break;

    case NR_PHP_FRANKENPHP_MODE_UNSUPPORTED:
      nr_php_frankenphp_thread.pending |= NR_PHP_FRANKENPHP_PENDING_UNSUPPORTED;
      nrl_warning(NRL_INIT,
                  "FrankenPHP " NRP_FMT
                  " isn't supported in worker mode "
                  "with " NR_PHP_FRANKENPHP_INI_WORKER_MODE "=" NRP_FMT_UQ
                  ", so the agent records no worker request transactions in "
                  "this worker lifetime",
                  NRP_CONFIG(version),
                  NRP_CONFIG(nr_php_frankenphp_setting_name(setting)));
      break;

    case NR_PHP_FRANKENPHP_MODE_INSTRUMENTED:
      if (!nr_php_frankenphp_parse_version(version, &parsed)
          && nr_php_frankenphp_log_once(
              NR_PHP_FRANKENPHP_LOG_UNKNOWN_VERSION)) {
        nrl_info(NRL_INIT,
                 "the agent can't read FrankenPHP's version " NRP_FMT
                 " as X.Y.Z; with " NR_PHP_FRANKENPHP_INI_WORKER_MODE
                 "=on it instruments worker mode anyway, which isn't "
                 "supported",
                 NRP_CONFIG(version));
      }
      break;
  }
}

/*
 * Classifies the script run from the request URI and FRANKENPHP_WORKER.
 *
 * The variable is read from $_SERVER first. Only when it isn't there and the
 * URI is bootstrap-shaped is it read from the SAPI's environment, the one case
 * in which that can change the answer. On FrankenPHP v1.11.2 sapi_getenv() is
 * a call into Go, too costly for every classic-mode RINIT.
 */
static nr_php_frankenphp_run_t nr_php_frankenphp_rinit_classify(
    const char* request_uri) {
  char* worker = nr_php_get_server_global("FRANKENPHP_WORKER");
  nr_php_frankenphp_run_t run;

  if (NULL == worker
      && NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV
             == nr_php_frankenphp_classify_run(request_uri, NULL)) {
    char* env = sapi_getenv(NR_PSTR("FRANKENPHP_WORKER"));

    run = nr_php_frankenphp_classify_run(request_uri, env);
    if (NULL != env) {
      efree(env);
    }
    return run;
  }

  run = nr_php_frankenphp_classify_run(request_uri, worker);
  nr_free(worker);
  return run;
}

void nr_php_frankenphp_rinit(void) {
  const char* request_uri = SG(request_info).request_uri;

  /*
   * What the last script run on this thread left pending, no transaction
   * consumed. It belongs to that worker lifetime, so drop it before this run
   * can send anything.
   */
  nr_php_frankenphp_thread.pending = 0;

  nr_php_frankenphp_thread.run = nr_php_frankenphp_rinit_classify(request_uri);
  nr_strlcpy(nr_php_frankenphp_thread.rinit_uri, request_uri,
             sizeof(nr_php_frankenphp_thread.rinit_uri));

  switch (nr_php_frankenphp_thread.run) {
    case NR_PHP_FRANKENPHP_RUN_BOOTSTRAP:
      nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_WORKER;
      nr_php_frankenphp_decide_mode();

      /* Under off nothing is recorded on a worker thread. */
      if (NR_PHP_FRANKENPHP_MODE_OFF == nr_php_frankenphp_thread.mode) {
        return;
      }

      /*
       * The bootstrap is named as soon as it begins, so that it keeps the
       * name however it ends: early, with newrelic_end_of_transaction(), or
       * at the end of a worker script that fails before its first
       * frankenphp_handle_request() call. The thread is a worker thread
       * before the transaction begins, so the begin makes it a background
       * job, as it does every transaction begun on a worker thread outside a
       * worker request. On an unsupported version the bootstrap is recorded
       * too, and carries the unsupported metric.
       */
      nr_php_txn_begin(0, 0);
      if (NULL != NRPRG(txn)) {
        nr_txn_set_path("FrankenPHP worker bootstrap", NRPRG(txn),
                        NR_PHP_FRANKENPHP_BOOTSTRAP_TXN_NAME,
                        NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
      }
      return;

    case NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET:
      if (nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_WORKER_ENV_SET)) {
        nrl_info(NRL_INIT,
                 "FRANKENPHP_WORKER is 1 for a request that isn't a worker "
                 "script's bootstrap; the agent takes it for classic mode");
      }
      break;

    case NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV:
      if (nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_NO_WORKER_ENV)) {
        nrl_info(NRL_INIT,
                 "the request URI " NRP_FMT
                 " looks like a worker script's, but FRANKENPHP_WORKER isn't "
                 "1; the agent takes the script run for classic mode",
                 NRP_URL(nr_php_frankenphp_thread.rinit_uri));
      }
      break;

    case NR_PHP_FRANKENPHP_RUN_CLASSIC:
      break;
  }

  nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_CLASSIC;
  nr_php_txn_begin(0, 0);
}

nr_php_frankenphp_thread_role_t nr_php_frankenphp_thread_role(void) {
  return nr_php_frankenphp_thread.role;
}

nr_php_frankenphp_mode_t nr_php_frankenphp_thread_mode(void) {
  return nr_php_frankenphp_thread.mode;
}

/*
 * Says, for the warning that RINIT missed a worker thread, which signals RINIT
 * saw.
 */
static const char* nr_php_frankenphp_run_signals(nr_php_frankenphp_run_t run) {
  switch (run) {
    case NR_PHP_FRANKENPHP_RUN_BOOTSTRAP:
      return "both signals";
    case NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET:
      return "FRANKENPHP_WORKER=1 without a worker script's URI";
    case NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV:
      return "a worker script's URI without FRANKENPHP_WORKER=1";
    case NR_PHP_FRANKENPHP_RUN_CLASSIC:
      break;
  }

  return "neither signal";
}

void nr_php_frankenphp_confirm_worker_thread(void) {
  nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_WORKER;

  /*
   * RINIT took a worker thread for classic mode, so a signal that the agent
   * reads from FrankenPHP has drifted. The metric goes on the next
   * transaction the thread sends, and the warning names what RINIT saw: the
   * URI it kept, because FrankenPHP updates SG(request_info) for each worker
   * request.
   */
  nr_php_frankenphp_thread.pending |= NR_PHP_FRANKENPHP_PENDING_MISSED_AT_RINIT;
  if (nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT)) {
    nrl_warning(
        NRL_INIT,
        "RINIT took a FrankenPHP worker thread for classic mode (" NRP_FMT_UQ
        ", request URI " NRP_FMT
        "); the agent records it as a worker thread from now on. The signals "
        "that FrankenPHP gives for a worker script may have changed",
        NRP_CONFIG(nr_php_frankenphp_run_signals(nr_php_frankenphp_thread.run)),
        NRP_URL(nr_php_frankenphp_thread.rinit_uri));
  }

  nr_php_frankenphp_decide_mode();
}

void nr_php_frankenphp_gate_rshutdown(void) {
  nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_UNKNOWN;
  nr_php_frankenphp_thread.mode = NR_PHP_FRANKENPHP_MODE_INSTRUMENTED;
  nr_php_frankenphp_thread.run = NR_PHP_FRANKENPHP_RUN_CLASSIC;
  nr_php_frankenphp_thread.rinit_uri[0] = '\0';
}

unsigned nr_php_frankenphp_thread_pending_metrics(void) {
  return nr_php_frankenphp_thread.pending;
}

/*
 * The bytes that a metric name segment may hold: ASCII letters and digits,
 * ., _, + and -. The ranges are spelled out, because isalnum() depends on the
 * locale. Every byte above 0x7F is outside them, so the name stays valid
 * UTF-8 even when the cut splits a character.
 */
static bool nr_php_frankenphp_metric_byte_allowed(unsigned char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')
         || (c >= 'a' && c <= 'z') || '.' == c || '_' == c || '+' == c
         || '-' == c;
}

char* nr_php_frankenphp_unsupported_metric_name(const char* version) {
  char segment[NR_PHP_FRANKENPHP_VERSION_METRIC_MAX + 1];
  size_t i;

  if (nr_strempty(version)) {
    return nr_strdup(NR_PHP_FRANKENPHP_METRIC_UNSUPPORTED "unknown");
  }

  for (i = 0; i < NR_PHP_FRANKENPHP_VERSION_METRIC_MAX && '\0' != version[i];
       i++) {
    segment[i]
        = nr_php_frankenphp_metric_byte_allowed((unsigned char)version[i])
              ? version[i]
              : '_';
  }
  segment[i] = '\0';

  return nr_formatf(NR_PHP_FRANKENPHP_METRIC_UNSUPPORTED "%s", segment);
}

void nr_php_frankenphp_txn_sending(nrtxn_t* txn) {
  if (NULL == txn) {
    return;
  }

  if (nr_php_frankenphp_thread.pending
      & NR_PHP_FRANKENPHP_PENDING_MISSED_AT_RINIT) {
    nrm_force_add(txn->unscoped_metrics,
                  NR_PHP_FRANKENPHP_METRIC_MISSED_AT_RINIT, 0);
  }

  if (nr_php_frankenphp_thread.pending
      & NR_PHP_FRANKENPHP_PENDING_UNSUPPORTED) {
    char* name = nr_php_frankenphp_unsupported_metric_name(
        NR_PHP_PROCESS_GLOBALS(frankenphp_version));

    nrm_force_add(txn->unscoped_metrics, name, 0);
    nr_free(name);
  }
}

void nr_php_frankenphp_txn_sent(void) {
  nr_php_frankenphp_thread.pending = 0;
}

#endif /* ZTS */
