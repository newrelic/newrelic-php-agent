/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_frankenphp_gate.h"
#include "php_agent.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_strings.h"

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

#endif /* ZTS */
