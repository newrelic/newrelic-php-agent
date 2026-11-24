/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp_gate.h"
#include "php_agent.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_threads.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#ifdef ZTS

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
  nr_php_frankenphp_run_t run;
  /*
   * The request URI seen at RINIT, truncated, and empty when there was none.
   * It is kept whatever its shape, because SG(request_info) can't be read
   * later instead: FrankenPHP updates it for each worker request.
   */
  char rinit_uri[256];
} nr_php_frankenphp_thread_t;

static nrt_thread_local nr_php_frankenphp_thread_t nr_php_frankenphp_thread
    = {.role = NR_PHP_FRANKENPHP_THREAD_UNKNOWN,
       .run = NR_PHP_FRANKENPHP_RUN_CLASSIC,
       .rinit_uri = ""};

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

  nr_php_frankenphp_thread.run = nr_php_frankenphp_rinit_classify(request_uri);
  nr_strlcpy(nr_php_frankenphp_thread.rinit_uri, request_uri,
             sizeof(nr_php_frankenphp_thread.rinit_uri));

  switch (nr_php_frankenphp_thread.run) {
    case NR_PHP_FRANKENPHP_RUN_BOOTSTRAP:
      /*
       * The bootstrap is named as soon as it begins, so that it keeps the
       * name however it ends: early, with newrelic_end_of_transaction(), or
       * at the end of a worker script that fails before its first
       * frankenphp_handle_request() call.
       */
      nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_WORKER;
      nr_php_txn_begin(0, 0);
      if (NULL != NRPRG(txn)) {
        nr_txn_set_path("FrankenPHP worker bootstrap", NRPRG(txn),
                        NR_PHP_FRANKENPHP_BOOTSTRAP_TXN_NAME,
                        NR_PATH_TYPE_CUSTOM, NR_OK_TO_OVERWRITE);
        nr_txn_set_as_background_job(NRPRG(txn), "FrankenPHP worker bootstrap");
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

void nr_php_frankenphp_confirm_worker_thread(void) {
  nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_WORKER;
}

void nr_php_frankenphp_gate_rshutdown(void) {
  nr_php_frankenphp_thread.role = NR_PHP_FRANKENPHP_THREAD_UNKNOWN;
  nr_php_frankenphp_thread.run = NR_PHP_FRANKENPHP_RUN_CLASSIC;
  nr_php_frankenphp_thread.rinit_uri[0] = '\0';
}

#endif /* ZTS */
