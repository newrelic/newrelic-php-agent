/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "nr_daemon_spawn.h"
#include "nr_daemon_spawn_private.h"
#include "util_errno.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_syscalls.h"

#define NR_DAEMON_ARGV_DEFAULT_CAPACITY 16
#define NR_DAEMON_ARGV_GROWTH_FACTOR 2

void nr_argv_append(nr_argv_t* argv, const char* flag_or_value) {
  if (argv->count >= argv->capacity) {
    size_t new_capacity;

    new_capacity = argv->capacity * NR_DAEMON_ARGV_GROWTH_FACTOR;
    if (new_capacity <= NR_DAEMON_ARGV_DEFAULT_CAPACITY) {
      new_capacity = NR_DAEMON_ARGV_DEFAULT_CAPACITY;
    }

    argv->data = (char**)nr_realloc(argv->data, new_capacity * sizeof(char*));
    argv->capacity = new_capacity;
  }

  if (flag_or_value) {
    argv->data[argv->count] = nr_strdup(flag_or_value);
  } else {
    argv->data[argv->count] = NULL;
  }

  argv->count++;
}

void nr_argv_append_flag(nr_argv_t* argv,
                         const char* flag,
                         const char* fmt,
                         ...) {
  int rv;
  va_list ap;
  char* tmp = NULL;

  if (NULL == fmt) {
    return;
  }

  va_start(ap, fmt);
  rv = vasprintf(&tmp, fmt, ap);
  va_end(ap);

  if (rv >= 0) {
    nr_argv_append(argv, flag);
    nr_argv_append(argv, tmp);
  }

  nr_free(tmp);
}

void nr_argv_destroy(nr_argv_t* argv) {
  size_t i;

  if (NULL == argv) {
    return;
  }

  for (i = 0; i < argv->count; i++) {
    nr_free(argv->data[i]);
  }

  nr_free(argv->data);
  argv->count = 0;
  argv->capacity = 0;
}

nr_argv_t* nr_daemon_args_to_argv(const char* name,
                                  const nr_daemon_args_t* args) {
  nr_argv_t* argv = NULL;

  argv = (nr_argv_t*)nr_zalloc(sizeof(*argv));
  nr_argv_append(argv, name);
  nr_argv_append(argv, "--agent");

  if (args) {
    nr_argv_append_flag(argv, "--pidfile", args->pidfile);
    nr_argv_append_flag(argv, "--logfile", args->logfile);
    nr_argv_append_flag(argv, "--loglevel", args->loglevel);
    nr_argv_append_flag(argv, "--auditlog", args->auditlog);

    if (0 != args->daemon_address) {
      nr_argv_append_flag(argv, "--port", "%s", args->daemon_address);
    }

    nr_argv_append_flag(argv, "--cafile", args->tls_cafile);
    nr_argv_append_flag(argv, "--capath", args->tls_capath);
    nr_argv_append_flag(argv, "--proxy", args->proxy);
    nr_argv_append_flag(argv, "--wait-for-port", args->start_timeout);

    if (args->app_timeout && ('\0' != args->app_timeout[0])) {
      nr_argv_append_flag(argv, "--define", "app_timeout=%s",
                          args->app_timeout);
    }

    /* utilization */
    nr_argv_append_flag(argv, "--define", "utilization.detect_aws=%s",
                        args->utilization.aws ? "true" : "false");
    nr_argv_append_flag(argv, "--define", "utilization.detect_azure=%s",
                        args->utilization.azure ? "true" : "false");
    nr_argv_append_flag(argv, "--define", "utilization.detect_gcp=%s",
                        args->utilization.gcp ? "true" : "false");
    nr_argv_append_flag(argv, "--define", "utilization.detect_pcf=%s",
                        args->utilization.pcf ? "true" : "false");
    nr_argv_append_flag(argv, "--define", "utilization.detect_docker=%s",
                        args->utilization.docker ? "true" : "false");
    nr_argv_append_flag(argv, "--define", "utilization.detect_kubernetes=%s",
                        args->utilization.kubernetes ? "true" : "false");

    /* diagnostic and testing flags */
    if (args->integration_mode) {
      nr_argv_append(argv, "--integration");
    }
  }

  /* Last element of the argument vector should be NULL. */
  nr_argv_append(argv, NULL);
  return argv;
}

#ifdef DO_GCOV
extern void __gcov_flush(void);
#endif

/*
 * Close all file descriptors greater than or equal to lowfd. Try to use
 * the most efficient method for the current platform.
 */
static void nr_closefrom(int lowfd) {
#if HAVE_CLOSEFROM
  closefrom(lowfd);
#elif HAVE_PROC_SELF_FD || HAVE_DEV_FD
  char path[PATH_MAX];
  char* endp;
  DIR* dirp;
  struct dirent* dent;
  int pathlen;
  int fd;

#if HAVE_PROC_SELF_FD
  pathlen = snprintf(path, sizeof(path), "/proc/%ld/fd", (long)getpid());
#else
  pathlen = snprintf(path, sizeof(path), "/dev/fd");
#endif

  if ((pathlen < 0) || (sizeof(path) < (size_t)pathlen)) {
    return;
  }

  dirp = opendir(path);
  if (0 == dirp) {
    return;
  }

  for (dent = readdir(dirp); dent; dent = readdir(dirp)) {
    fd = strtol(dent->d_name, &endp, 10);
    if ((dent->d_name == endp) || (0 != *endp)) {
      continue;
    }
    if ((fd < lowfd) || (fd == dirfd(dirp))) {
      continue;
    }

    (void)nr_close(fd);
  }

  (void)closedir(dirp);
#else
  int fd;
  int maxfd;

  maxfd = sysconf(_SC_OPEN_MAX);
  if ((maxfd < 0) || (maxfd > 64 * 1024)) {
    maxfd = 64 * 1024;
  }

  for (fd = lowfd; fd < maxfd; ++fd) {
    (void)close(fd);
  }
#endif
}

/*
 * Hooks for replacing fork and exec syscalls during testing.
 */
pid_t (*nr_daemon_fork_hook)(void) = fork;
int (*nr_daemon_execvp_hook)(const char* path, char* const argv[]) = execvp;

pid_t nr_spawn_daemon(const char* path, const nr_daemon_args_t* args) {
  pid_t dpid;
  int ret;
  int logfd = -1;
  int nullfd = -1;
  size_t i;
  nr_argv_t* argv = NULL;

  if (0 == args) {
    nrl_warning(NRL_INIT, "no daemon arguments given");
    errno = EINVAL;
    return -1;
  }

  if ((0 == path) || (0 == *path)) {
    nrl_warning(NRL_INIT, "no daemon location specified");
    errno = EINVAL;
    return -1;
  }

  if (-1 == nr_access(path, X_OK)) {
    nrl_warning(NRL_INIT, "couldn't find daemon=" NRP_FMT " (%.16s)",
                NRP_FILENAME(path), nr_errno(errno));
    return -1;
  }

  dpid = nr_daemon_fork_hook();

  if (-1 == dpid) {
    nrl_error(NRL_INIT, "failed to fork daemon errno=%.16s", nr_errno(errno));
    return -1;
  }

  if (0 != dpid) {
    int status;

    /* This is the parent process. */
    nrl_info(NRL_INIT, "spawned daemon child pid=%d", (int)dpid);

    /*
     * Wait for the daemon process to double fork and detach into its own
     * session. This prevents a defunct daemon process from hanging around,
     * and also prevents spurious ALERT messages in the PHP FPM log because
     * the daemon is not recognized as an FPM worker.
     */
    (void)waitpid(dpid, &status, 0);

    return dpid;
  }

  /* Redirect stdin to /dev/null */
  nullfd = nr_open("/dev/null", O_RDWR, 0666);
  if (-1 != nullfd) {
    nr_dup2(nullfd, 0);
    nr_close(nullfd);
  }

  /*
   * The daemon prints errors to stderr until it successfully
   * initializes its own log file. Redirect stdout and stderr
   * to the agent log in the meanwhile so no output is lost.
   */
  logfd = nrl_get_log_fd();
  if (-1 != logfd) {
    nr_dup2(logfd, 1);
    nr_dup2(logfd, 2);
  }

  /*
   * Log final arguments for daemon before closing the log file.
   */
  argv = nr_daemon_args_to_argv(path, args);
  for (i = 0; i < argv->count; i++) {
    nrl_verbosedebug(NRL_INIT, "exec[%zu]=" NRP_FMT, i,
                     NRP_PROCARG(argv->data[i]));
  }

  /*
   * Do not inherit any additional file descriptors from this process.
   */
  nr_closefrom(3);

  /*
   * Flush gcov counters so they aren't lost when this address space
   * is overwritten by exec.
   */
#ifdef DO_GCOV
  __gcov_flush();
#endif

  ret = nr_daemon_execvp_hook(path, argv->data);
  if (0 != ret) {
    nrl_warning(NRL_INIT,
                "failed to spawn daemon: (%.16s) - please start it manually",
                nr_errno(errno));
  }
  _exit(0);
}
