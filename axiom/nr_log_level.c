/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_log_level.h"
#include "util_strings.h"
#include "util_logging.h"
#include "util_memory.h"

int nr_log_level_str_to_int(const char* str) {
  int level;
  bool err = false;

  if (NULL == str) {
    err = true;
  }

#define LEVELCMP(x) (0 == nr_stricmp(str, x))

  if (!err) {
    if (LEVELCMP(LL_EMER_STR)) {
      level = LOG_LEVEL_EMERGENCY;
    } else if (LEVELCMP(LL_ALER_STR)) {
      level = LOG_LEVEL_ALERT;
    } else if (LEVELCMP(LL_CRIT_STR)) {
      level = LOG_LEVEL_CRITICAL;
    } else if (LEVELCMP(LL_ERRO_STR)) {
      level = LOG_LEVEL_ERROR;
    } else if (LEVELCMP(LL_WARN_STR)) {
      level = LOG_LEVEL_WARNING;
    } else if (LEVELCMP(LL_NOTI_STR)) {
      level = LOG_LEVEL_NOTICE;
    } else if (LEVELCMP(LL_INFO_STR)) {
      level = LOG_LEVEL_INFO;
    } else if (LEVELCMP(LL_DEBU_STR)) {
      level = LOG_LEVEL_DEBUG;
    } else {
      err = true;
    }
  }

#undef LEVELCMP

  if (err) {
    nrl_warning(
        NRL_INIT,
        "Unknown Log Forwarding Log Level Specified; Defaulting to \"%s\"",
        nr_log_level_rfc_to_psr(LOG_LEVEL_UNKNOWN));
    level = LOG_LEVEL_UNKNOWN;
  }

  return level;
}

const char* nr_log_level_rfc_to_psr(int level) {
  const char* psr = NULL;

  switch (level) {
    case LOG_LEVEL_EMERGENCY:
      psr = LL_EMER_STR;
      break;
    case LOG_LEVEL_ALERT:
      psr = LL_ALER_STR;
      break;
    case LOG_LEVEL_CRITICAL:
      psr = LL_CRIT_STR;
      break;
    case LOG_LEVEL_ERROR:
      psr = LL_ERRO_STR;
      break;
    case LOG_LEVEL_WARNING:
      psr = LL_WARN_STR;
      break;
    case LOG_LEVEL_NOTICE:
      psr = LL_NOTI_STR;
      break;
    case LOG_LEVEL_INFO:
      psr = LL_INFO_STR;
      break;
    case LOG_LEVEL_DEBUG:
      psr = LL_DEBU_STR;
      break;
    default:
      psr = LL_UNKN_STR;
      break;
  }

  return psr;
}
