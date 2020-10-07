/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for common signal handling.
 */
#ifndef UTIL_SIGNALS_HDR
#define UTIL_SIGNALS_HDR

/*
 * Purpose : Re-raises the given signal with the default signal handler
 *           provided by the operating system. Useful to ensure core dumps are
 *           generated where appropriate. Note that this function doesn't call
 *           exit(), so if called with a non-fatal signal execution will
 *           continue.
 *
 * Params  : 1. The signal to re-raise.
 *
 * Returns : Nothing.
 *
 * Warning : 1. This function does not call exit(). For fatal signals, this
 *              isn't an issue, but this function is probably inappropriate for
 *              non-fatal signals.
 *
 *           2. This function will replace the signal handler for sig with
 *              SIG_DFL. Of course, this is only a problem if you're calling
 *              this function for a non-fatal signal, in which case see warning
 *              1 above.
 */
extern void nr_signal_reraise(int sig);

/*
 * Purpose : Used to prepare a process for common fatal signal handling. This
 *           must be called after the log file has been opened, and is most
 *           useful if called when, for example, the SIGSEGV, SIGBUS, SIGFPE or
 *           SIGILL handler is being installed.
 *
 * Params  : None.
 *
 * Returns : Nothing.
 */
extern void nr_signal_tracer_prep(void);

/*
 * Purpose : Common signal handling that prints a stack dump if the system
 *           supports that. Will do nothing if nr_signal_tracer_prep() above
 *           has not been called.
 *
 * Params  : 1. The signal number.
 *
 * Returns : Nothing.
 */
extern void nr_signal_tracer_common(int sig);

/*
 * Purpose : Install a handler for a variety of fatal signals.
 */
extern void nr_signal_handler_install(void (*handler)(int sig));

#endif /* UTIL_SIGNALS_HDR */
