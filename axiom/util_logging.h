/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for log message and log file handling.
 */
#ifndef UTIL_LOGGING_HDR
#define UTIL_LOGGING_HDR

#include <stdarg.h>
#include <stdint.h>

#include "nr_axiom.h"

/*
 * The various log levels.
 */
typedef enum _nrloglev_t {
  NRL_ALWAYS = 0,
  NRL_ERROR,
  NRL_WARNING,
  NRL_INFO,
  NRL_VERBOSE,
  NRL_DEBUG,
  NRL_VERBOSEDEBUG,
  NRL_HIGHEST_LEVEL
} nrloglev_t;

/*
 * The various subsystems.
 */
#define NRL_AUTORUM 0x00000001
#define NRL_METRICS 0x00000002
#define NRL_HARVESTER 0x00000004
#define NRL_RPM 0x00000008
#define NRL_INSTRUMENT 0x00000010
#define NRL_FRAMEWORK 0x00000020
#define NRL_NETWORK 0x00000040
#define NRL_LISTENER 0x00000080
#define NRL_DAEMON 0x00000100
#define NRL_INIT 0x00000200
#define NRL_SHUTDOWN 0x00000400
#define NRL_MEMORY 0x00000800
#define NRL_STRING 0x00001000
#define NRL_SEGMENT 0x00002000
#define NRL_THREADS 0x00004000
#define NRL_API 0x00008000
#define NRL_IPC 0x00010000
#define NRL_TXN 0x00020000
#define NRL_RULES 0x00040000
#define NRL_ACCT 0x00080000
#define NRL_CONNECTOR 0x00100000
#define NRL_SQL 0x00200000
#define NRL_AGENT 0x00400000
#define NRL_CAT 0x00800000
#define NRL_MISC 0x20000000
#define NRL_TEST 0x40000000
#define NRL_NRPROF 0x80000000
#define NRL_ALL_FLAGS 0x7fffffff

/*
 * Exposed for testing and the nrl_should_print inline function.
 */
extern const uint32_t* const nrl_level_mask_ptr;

static inline int nrl_should_print(nrloglev_t level, uint32_t subsystem) {
  if (NRL_ALWAYS == level) {
    return 1;
  }
  if (nrl_level_mask_ptr[level] & subsystem) {
    return 1;
  }
  return 0;
}

  /*
   * Macros to mediate the way that strings are printed into the logfile.
   * We need to mediate strings so that we aren't susceptible to logfile
   * injection attacks.
   *
   * These macros are used to make arguments to printf-like functions.
   * These macros create two sequential arguments to printf-like functions.
   * Do NOT wrap the macro expansion into (), as that will create a ',' operator
   * (sequential evaluation, left to right) which evaluates to one argument.
   *
   * The corresponding format directive must be a single %.*s,
   * which ways to pick up a numeric value as the field width,
   * and then clamp the printed length of the string to be no more than the
   * given value. Use the NRP_FMT macro with ANSI string concatenation to build
   * up format specifiers.
   *
   * If you choose to call a function to scrub the value being printed
   * (for example to take a char * and return a pointer to a buffer
   * holding the scrubbed value), bear in mind that while the lifetime of
   * the value returned from the scrubbing function is small, there may
   * be several such values outstanding before they are printed/logged.
   *
   * In other words, be careful with the use of static buffers to save on
   * allocation time.
   *
   * There should be no instances of this regular expression in the source code:
   *   nrl_.*%[0-9]*s
   * eg, no string formatting with unbounded length.
   */

#define NRP_APPNAME(N) 48, NRSAFESTR(N)    /* application name */
#define NRP_FWNAME(N) 48, NRSAFESTR(N)     /* framework name */
#define NRP_TXNNAME(N) 150, NRSAFESTR(N)   /* transaction name */
#define NRP_LICNAME(N) 40, NRSAFESTR(N)    /* license name */
#define NRP_HOSTNAME(N) 80, NRSAFESTR(N)   /* host name */
#define NRP_METRICNAME(N) 80, NRSAFESTR(N) /* host name */
#define NRP_MIME(N) 80, NRSAFESTR(N)       /* mime type */
#define NRP_URL(N) 400, NRSAFESTR(N)       /* URL */
#define NRP_SQL(N) 400, NRSAFESTR(N)       /* SQL query */
#define NRP_REGEXP(N) 100, NRSAFESTR(N)    /* regular expression */
#define NRP_CONNECT(N)                                                       \
  256, NRSAFESTR(N) /* echoing connect messages with text from the collector \
                     */
#define NRP_RUM(N) 2000, NRSAFESTR(N)    /* auto rum */
#define NRP_CAT(N) 512, NRSAFESTR(N)     /* CAT header */
#define NRP_JSON(N) 200000, NRSAFESTR(N) /* json */
#define NRP_PHP(N)                                                        \
  100, NRBLANKSTR(N) /* names derived from PHP engine, such as functions, \
                        namespaces, modules, etc */
#define NRP_ARGUMENTS(N) \
  100, NRBLANKSTR(N) /* arguments given to PHP functions */
#define NRP_ARGSTR(N) \
  80, (N) /* arguments given to PHP functions, copied into local buffer */
#define NRP_FILENAME(N)                                                        \
  250, NRSAFESTR(N) /* source code file names (these tend to be long in modern \
                       frameworks) */
#define NRP_PROCARG(N) \
  300, NRSAFESTR(N) /* arguments given to process invocation */
#define NRP_CONFIG(N) \
  256, NRSAFESTR(N) /* internally created agent configuration string */
#define NRP_BUFFER(N) \
  ((int)sizeof(N)),   \
      (N) /* local char[] array; NRSAFESTR doesn't work on such beasts */
#define NRP_ERROR(N) \
  256, NRSAFESTR(N) /* error type and info from PHP exception frame */
#define NRP_COMMAND(N) \
  16, (N) /* internal command name; doesn't need to be safed */
#define NRP_STRERROR(N) \
  64, (N) /* string from strerror, which will be non-null */

/*
 * Use one of these format directive strings to build up format directives
 * to print the result of the above macros.
 *
 * NRP_FMT quotes the string using '' in order to delimit the scope of the
 * string. NRP_FMT_UQ doesn't quote the string.  Use this sparingly, as for
 * example when printing banner information.
 */
#define NRP_FMT "'%.*s'"
#define NRP_FMT_UQ "%.*s"

/*
 * Purpose : Open or close a log file.  Note that the log file descriptors
 *           are not mutex protected.  Therefore, these functions should be
 *           called by a single thread before and after logging occurs.
 */
extern nr_status_t nrl_set_log_file(const char* filename);
extern void nrl_close_log_file(void);

/*
 * Purpose : Return the fd of the log file for direct writing / dumping.
 *
 * Returns : The fd or -1 if no log file is being used at the moment.
 */
extern int nrl_get_log_fd(void);

/*
 * Purpose : Send a message at the specified level to the log file.
 *
 * Params  : 1. The log level.
 *           2. The log format string (printf-style)
 *           3... any arguments required by the format string.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 *
 * Notes   : This function is NOT meant to be called directly. The intention is
 *           that all log messages are sent through the macros defined below,
 *           as they check that the subsystem in question has logging at the
 *           desired level enabled.
 *
 * Notes   : This function may not be called safely from within
 *           a signal handler, since it calls* functions,
 *           such as vasprintf, which call malloc/free; malloc/free
 *           are not safe to call from within a signal handler.
 *           Further, even the supposedly malloc-free library routine to
 *           capture backtraces allocates memory when the corresponding
 *           DSO is loaded
 */
extern nr_status_t nrl_send_log_message(nrloglev_t level, const char* fmt, ...)
    NRPRINTFMT(2);

/*
 * Purpose : Set the log level for all or specific subsystems
 *
 * Params  : A string with the log level(s) to set. If this is NULL or empty,
 *           return to the default INFO level. Otherwise, it can be a comma
 *           separated list of either a level or subsystem=level settings, or
 *           both. The level is the traditional error, warning, info, verbose,
 *           debug or verbosedebug. The subsystem is one of the subsystems
 *           defined above. Some examples of valid log level settings:
 *           info - the default - set all subsystems to the info level
 *           debug - set all subsystem to the debug level
 *           verbose,api=debug,daemon=verbosedebug - sets all subsystems to the
 *           verbose level, then selectively sets log messages related to API
 *           calls and daemon communication to the debug and verbosedebug
 *           levels, respectively.
 *
 * Returns : NR_SUCCESS or NR_FAILURE (which usually means reset to INFO).
 */
extern nr_status_t nrl_set_log_level(const char* level);

#define nrl_always(...) nrl_send_log_message(NRL_ALWAYS, __VA_ARGS__)

#define nrl_error(M, ...)                           \
  do {                                              \
    if (nrl_should_print(NRL_ERROR, (M))) {         \
      nrl_send_log_message(NRL_ERROR, __VA_ARGS__); \
    }                                               \
  } while (0)

#define nrl_warning(M, ...)                           \
  do {                                                \
    if (nrl_should_print(NRL_WARNING, (M))) {         \
      nrl_send_log_message(NRL_WARNING, __VA_ARGS__); \
    }                                                 \
  } while (0)

#define nrl_info(M, ...)                           \
  do {                                             \
    if (nrl_should_print(NRL_INFO, (M))) {         \
      nrl_send_log_message(NRL_INFO, __VA_ARGS__); \
    }                                              \
  } while (0)

#define nrl_verbose(M, ...)                           \
  do {                                                \
    if (nrl_should_print(NRL_VERBOSE, (M))) {         \
      nrl_send_log_message(NRL_VERBOSE, __VA_ARGS__); \
    }                                                 \
  } while (0)

#define nrl_debug(M, ...)                           \
  do {                                              \
    if (nrl_should_print(NRL_DEBUG, (M))) {         \
      nrl_send_log_message(NRL_DEBUG, __VA_ARGS__); \
    }                                               \
  } while (0)

#define nrl_verbosedebug(M, ...)                           \
  do {                                                     \
    if (nrl_should_print(NRL_VERBOSEDEBUG, (M))) {         \
      nrl_send_log_message(NRL_VERBOSEDEBUG, __VA_ARGS__); \
    }                                                      \
  } while (0)

/*
 * Purpose : Write a log message, but using a va_list rather than variable
 * number of arguments.
 */
extern void nrl_vlog(nrloglev_t level,
                     uint32_t subsystem,
                     const char* fmt,
                     va_list ap);

#endif /* UTIL_LOGGING_HDR */
