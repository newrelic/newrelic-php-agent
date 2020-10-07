/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/time.h>

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

#include "util_logging.h"
#include "util_logging_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_syscalls.h"

typedef struct _nrl_subsys_names_t {
  const char* name;
  uint32_t maskval;
} nrl_subsys_names_t;

static nrl_subsys_names_t subsys_names[] = {{"autorum", NRL_AUTORUM},
                                            {"metrics", NRL_METRICS},
                                            {"harvester", NRL_HARVESTER},
                                            {"rpm", NRL_RPM},
                                            {"instrument", NRL_INSTRUMENT},
                                            {"framework", NRL_FRAMEWORK},
                                            {"network", NRL_NETWORK},
                                            {"listener", NRL_LISTENER},
                                            {"daemon", NRL_DAEMON},
                                            {"init", NRL_INIT},
                                            {"shutdown", NRL_SHUTDOWN},
                                            {"memory", NRL_MEMORY},
                                            {"string", NRL_STRING},
                                            {"segment", NRL_SEGMENT},
                                            {"threads", NRL_THREADS},
                                            {"api", NRL_API},
                                            {"ipc", NRL_IPC},
                                            {"txn", NRL_TXN},
                                            {"transaction", NRL_TXN},
                                            {"rules", NRL_RULES},
                                            {"acct", NRL_ACCT},
                                            {"account", NRL_ACCT},
                                            {"connector", NRL_CONNECTOR},
                                            {"sql", NRL_SQL},
                                            {"agent", NRL_AGENT},
                                            {"cat", NRL_CAT},
                                            {"test", NRL_TEST},
                                            {"misc", NRL_MISC},
                                            {"*", NRL_ALL_FLAGS},
                                            {"all", NRL_ALL_FLAGS}};
static int num_subsys_names = sizeof(subsys_names) / sizeof(nrl_subsys_names_t);

static const char* level_names[NRL_HIGHEST_LEVEL] = {
    "always",      /* NRL_ALWAYS */
    "error",       /* NRL_ERROR */
    "warning",     /* NRL_WARNING */
    "info",        /* NRL_INFO */
    "verbose",     /* NRL_VERBOSE */
    "debug",       /* NRL_DEBUG */
    "verbosedebug" /* NRL_VERBOSEDEBUG */
};

static uint32_t nrl_level_mask[NRL_HIGHEST_LEVEL] = {
    NRL_ALL_FLAGS, /* NRL_ALWAYS */
    NRL_ALL_FLAGS, /* NRL_ERROR */
    NRL_ALL_FLAGS, /* NRL_WARNING */
    NRL_ALL_FLAGS, /* NRL_INFO */
    0,             /* NRL_VERBOSE */
    0,             /* NRL_DEBUG */
    0              /* NRL_VERBOSEDEBUG */
};

static int logfile_fd = -1;

const uint32_t* const nrl_level_mask_ptr = nrl_level_mask;

nr_status_t nrl_set_log_file(const char* filename) {
  if ((0 == filename) || (0 == filename[0])) {
    return NR_FAILURE;
  }

  /*
   * Close an existing log file, if one is open.
   */
  if (-1 != logfile_fd) {
    nr_close(logfile_fd);
  }

  if (0 == nr_strcmp("stdout", filename)) {
    logfile_fd = nr_dup(1);
  } else if (0 == nr_strcmp("stderr", filename)) {
    logfile_fd = nr_dup(2);
  } else {
    logfile_fd = nr_open(filename, O_WRONLY | O_APPEND | O_CREAT, 0666);
  }

  if (-1 == logfile_fd) {
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

void nrl_close_log_file(void) {
  if (-1 == logfile_fd) {
    return;
  }
  nr_close(logfile_fd);
  logfile_fd = -1;
}

int nrl_get_log_fd(void) {
  return logfile_fd;
}

void nrl_format_timestamp(char* buf, size_t buflen, const struct timeval* tv) {
  struct tm tm;
  int offset_secs; /* difference from GMT in seconds */
  int offset_24h;  /* difference in 24h format e.g. -0700 */

  localtime_r(&tv->tv_sec, &tm);

#ifdef NR_SYSTEM_SOLARIS
  /*
   * Solaris keeps the GMT offset in a pair of globals. The correct
   * choice depends on whether daylight savings is in effect. The
   * sign is also reversed compared to POSIX. i.e. a negative offset
   * indicates a timezone that is east of Greenwich rather than west.
   */
  offset_secs = -(int)(tm.tm_isdst && daylight ? altzone : timezone);
#else
  offset_secs = (int)tm.tm_gmtoff;
#endif

  if (offset_secs >= 0) {
    const int hours = (offset_secs / 60) / 60;
    const int minutes = (offset_secs / 60) % 60;
    offset_24h = hours * 100 + minutes;
  } else {
    /*
     * Note: the modulus operator rounds towards zero, therefore
     * we must convert offset_secs to a positive number to correctly
     * calculate the minutes. e.g. -5 % 3 == -1 NOT -2.
     */
    const int hours = (-offset_secs / 60) / 60;
    const int minutes = (-offset_secs / 60) % 60;
    offset_24h = -(hours * 100 + minutes);
  }

  buf[0] = '\0';
  snprintf(buf, buflen, "%04d-%02d-%02d %02d:%02d:%02d.%03d %+05d",
           (int)tm.tm_year + 1900, (int)tm.tm_mon + 1, (int)tm.tm_mday,
           (int)tm.tm_hour, (int)tm.tm_min, (int)tm.tm_sec,
           (int)(tv->tv_usec / 1000), offset_24h);
}

static char logger_newline[]
    = "\n"; /* must be static char to be used in iovec */

static nr_status_t nrl_send_log_message_internal(int fd,
                                                 nrloglev_t level,
                                                 const char* fmt,
                                                 va_list ap) {
  char preamble[128];
  struct iovec miov[3];
  struct timeval tv;
  char log_timestamp[128];
  char* msg;
  int preamble_len;
  int msg_len;
  ssize_t write_rv;

  if ((int)level < (int)NRL_ALWAYS) {
    return NR_FAILURE;
  }
  if ((int)level >= (int)NRL_HIGHEST_LEVEL) {
    return NR_FAILURE;
  }

  if (-1 == fd) {
    return NR_FAILURE;
  }

  tv.tv_sec = 0;
  gettimeofday(&tv, 0);
  log_timestamp[0] = '\0';
  nrl_format_timestamp(log_timestamp, sizeof(log_timestamp), &tv);

  preamble_len
      = snprintf(preamble, sizeof(preamble), "%s (%d %d) %s: ", log_timestamp,
                 nr_getpid(), nr_gettid(), level_names[level]);

  if (-1 == preamble_len) {
    return NR_FAILURE;
  }

  msg = NULL;
  msg_len = vasprintf(&msg, fmt, ap);

  if (-1 == msg_len) {
    return NR_FAILURE;
  }

  miov[0].iov_base = preamble;
  miov[0].iov_len = (size_t)preamble_len;

  miov[1].iov_base = msg;
  miov[1].iov_len = (size_t)msg_len;

  miov[2].iov_base = logger_newline;
  miov[2].iov_len = 1;

  write_rv = nr_writev(fd, miov, 3);

  nr_free(msg);

  if (-1 == write_rv) {
    return NR_FAILURE;
  } else {
    return NR_SUCCESS;
  }
}

nr_status_t nrl_send_log_message(nrloglev_t level, const char* fmt, ...) {
  nr_status_t rv;
  va_list ap;

  va_start(ap, fmt);
  rv = nrl_send_log_message_internal(logfile_fd, level, fmt, ap);
  va_end(ap);

  return rv;
}

static void set_all_up_to(nrloglev_t level, uint32_t flags) {
  int i;

  for (i = (int)NRL_ERROR; i < (int)NRL_HIGHEST_LEVEL; i++) {
    if (i <= (int)level) {
      nrl_level_mask[i] |= flags;
    } else {
      nrl_level_mask[i] &= ~flags;
    }
  }
}

nr_status_t nrl_set_log_level(const char* level) {
  nrobj_t* args = NULL;
  char* ep;
  int nargs = 0;
  int i;
  int j;

  nr_memset(nrl_level_mask, 0, sizeof(nrl_level_mask));
  nrl_level_mask[NRL_ALWAYS] = NRL_ALL_FLAGS;

  if ((0 == level) || (0 == level[0])) {
    level = "info";
  }

  args = nr_strsplit(level, ",;", 0);
  nargs = nro_getsize(args);
  if ((0 == args) || (nargs <= 0)) {
    goto error;
  }

  for (i = 0; i < nargs; i++) {
    const char* s = nro_get_array_string(args, i + 1, NULL);
    ep = nr_strchr(s, '=');

    if (0 == ep) {
      for (j = (int)NRL_ERROR; j < (int)NRL_HIGHEST_LEVEL; j++) {
        if (0 == nr_stricmp(s, level_names[j])) {
          set_all_up_to((nrloglev_t)j, NRL_ALL_FLAGS);
          break;
        }
      }

      if ((int)NRL_HIGHEST_LEVEL == j) {
        /*
         * Invalid level passed. Return failure.
         */
        goto error;
      }
    } else {
      uint32_t mask = 0;
      *ep = 0;
      ep++;

      for (j = 0; j < num_subsys_names; j++) {
        if (0 == nr_strcmp(s, subsys_names[j].name)) {
          mask = subsys_names[j].maskval;
          break;
        }
      }

      if (num_subsys_names == j) {
        /*
         * Invalid subsystem passed. Return failure.
         */
        goto error;
      }

      for (j = (int)NRL_ERROR; j < (int)NRL_HIGHEST_LEVEL; j++) {
        if (0 == nr_stricmp(ep, level_names[j])) {
          set_all_up_to((nrloglev_t)j, mask);
          break;
        }
      }

      if ((int)NRL_HIGHEST_LEVEL == j) {
        /*
         * Invalid level passed. Return failure.
         */
        goto error;
      }
    }
  }

  nro_delete(args);
  return NR_SUCCESS;

error:
  nro_delete(args);
  set_all_up_to(NRL_INFO, NRL_ALL_FLAGS);
  return NR_FAILURE;
}

void nrl_vlog(nrloglev_t level,
              uint32_t subsystem,
              const char* fmt,
              va_list ap) {
  if (0 == nrl_should_print(level, subsystem)) {
    return;
  }

  nrl_send_log_message_internal(logfile_fd, level, fmt, ap);
}
