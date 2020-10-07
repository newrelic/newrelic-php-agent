/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DAEMON_PRIVATE_HDR
#define NR_DAEMON_PRIVATE_HDR

#include <sys/types.h>

#include "nr_daemon_spawn.h"

/*
 * nr_argv_t is a simple builder for constructing the argument
 * vector for the daemon.
 */
typedef struct _nr_argv_t {
  char** data;
  size_t count;
  size_t capacity;
} nr_argv_t;

/*
 * Hook for testing nr_spawn_daemon(). Defaults to the real fork function,
 * tests should be careful to restore the original value.
 */
extern pid_t (*nr_daemon_fork_hook)(void);

/*
 * Hook for testing nr_spawn_daemon(). Defaults to the real execvp function,
 * tests should be careful to restore the original value.
 */
extern int (*nr_daemon_execvp_hook)(const char* path, char* const argv[]);

/*
 * Purpose : Append a value to argv.
 *
 * Params  : 1. The argument vector to modify.
 *           2. The flag or value to append. NULL is allowed, but should be
 *              reserved for the final argument as per POSIX conventions.
 *
 * See: http://pubs.opengroup.org/onlinepubs/009695399/functions/exec.html
 */
extern void nr_argv_append(nr_argv_t* argv, const char* flag_or_value);

extern void nr_argv_append_flag(nr_argv_t* argv,
                                const char* flag,
                                const char* fmt,
                                ...);

/*
 * Purpose : Free resources associated with argv. The caller is responsible
 *           for freeing argv itself if it was heap allocated.
 *
 * Params  : 1. The argument vector to destroy.
 *
 * Notes   : This function is idempotent, and it is safe to reuse argv after.
 */
extern void nr_argv_destroy(nr_argv_t* argv);

extern nr_argv_t* nr_daemon_args_to_argv(const char* name,
                                         const nr_daemon_args_t* args);

#endif /* NR_DAEMON_PRIVATE_HDR */
