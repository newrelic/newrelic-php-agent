/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Be careful: some signals, such as SIGSEGV, SIGFPE, SIGILL or SIGBUS,
 * are non recoverable in a portable environment.
 */
#include "nr_axiom.h"

#include <dlfcn.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <signal.h>
#include <stdio.h>

#include "nr_version.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_strings.h"
#include "util_syscalls.h"

static int signal_tracer_fd = -1;
static char signal_tracer_banner[256];
static size_t signal_tracer_bannerlen = 0;

#ifdef HAVE_BACKTRACE
#define NUM_BACKTRACE_FRAMES 100
#endif

void nr_signal_reraise(int sig) {
  struct sigaction sa;

  nr_memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  sigfillset(&sa.sa_mask);
  sigaction(sig, &sa, 0);
  raise(sig);
}

void nr_signal_tracer_prep(void) {
  Dl_info info;
  void* base_addr;

  base_addr = nr_signal_tracer_prep;
  if (dladdr(nr_signal_tracer_prep, &info)) {
    base_addr = info.dli_fbase;
  }

  signal_tracer_fd = nrl_get_log_fd();

  if (signal_tracer_fd >= 0) {
    signal_tracer_bannerlen
        = snprintf(signal_tracer_banner, sizeof(signal_tracer_banner),
                   "process id %d fatal signal (SIGSEGV, SIGFPE, SIGILL, "
                   "SIGBUS, ...)  - stack dump follows (code=%p bss=%p):\n",
                   nr_getpid(), base_addr, &signal_tracer_fd);
  }
}

/*
 * WATCH OUT! There be dragons here.
 *
 * Do the bare amount of work to trace a fatal signal.
 *
 * Do NOT transitively call* malloc,
 * as it is unsafe to call malloc from a signal handler.
 *
 * Note that calling a library function from here
 * may invoke the dynamic linker if that libary function
 * is in its own DSO.
 */
void nr_signal_tracer_common(int sig) {
  char buf[256];
  const char* signal_name = 0;

  if (signal_tracer_fd < 0) {
    return;
  }

  if (SIGSEGV == sig) {
    signal_name = "segmentation violation";
  } else if (SIGFPE == sig) {
    signal_name = "SIGFPE: likely integer zero divide";
  } else if (SIGBUS == sig) {
    signal_name = "SIGBUS";
  } else if (SIGILL == sig) {
    signal_name = "SIGILL";
  } else {
    signal_name = "?";
  }
  snprintf(buf, sizeof(buf),
           "Process %d (version %s) received signal %2d: %s\n", nr_getpid(),
           nr_version(), sig, signal_name);
  nr_write(signal_tracer_fd, buf, nr_strlen(buf));
  nr_write(signal_tracer_fd, signal_tracer_banner, signal_tracer_bannerlen);

#ifdef HAVE_BACKTRACE
  /*
   * These calls may call malloc, either as part of getting
   * their work done, or when the dynamic linker loads
   * the owning DSO into memory.
   */
  {
    void* array[NUM_BACKTRACE_FRAMES];
    size_t size;

    size = backtrace(array, NUM_BACKTRACE_FRAMES);
    backtrace_symbols_fd(array, size, signal_tracer_fd);
  }
#else
  {
    snprintf(buf, sizeof(buf), "No backtrace on this platform.\n");
    nr_write(signal_tracer_fd, buf, nr_strlen(buf));
  }
#endif
}

/*
 * This signal handler MUST eventually exit or abort.
 * Some of the signals it handles, such as SIGFPE, are non recoverable
 * in compliant programs.
 */
static void default_fatal_signal_handler(int sig) {
  nr_signal_tracer_common(sig);

  /*
   * Reraise the signal with the default signal handler so that the OS can dump
   * core or perform any other configured action.
   */
  nr_signal_reraise(sig);
}

void nr_signal_handler_install(void (*handler)(int sig)) {
  struct sigaction sa;

  nr_signal_tracer_prep();

  nr_memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler ? handler : default_fatal_signal_handler;
  sigfillset(&sa.sa_mask);
  sigaction(SIGSEGV, &sa, 0);
  sigaction(SIGBUS, &sa, 0);
  sigaction(SIGFPE, &sa, 0);
  sigaction(SIGILL, &sa, 0);
  sigaction(SIGABRT, &sa, 0);
}
