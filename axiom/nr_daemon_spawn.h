/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to manage the agent's connection to the daemon.
 */
#ifndef NR_DAEMON_HDR
#define NR_DAEMON_HDR

#include <sys/types.h>
#include "nr_utilization.h"

typedef struct _nr_daemon_args_t {
  const char* pidfile;  /* daemon process id file location */
  const char* logfile;  /* daemon log file location */
  const char* loglevel; /* daemon log level */
  const char* auditlog; /* daemon audit log file location */

  /*
   * Options affecting communication with the daemon.
   */
  const char* daemon_address; /* address of the daemon; a string representing
                                 UDS, abstract socket, or port. */
  /*
   * Options affecting how the daemon connects to and communicates with New
   * Relic.
   */
  const char* proxy;      /* connect through a proxy server */
  int tls_enabled;        /* always use a secure connection */
  const char* tls_cafile; /* use a custom X509 certificate bundle for host
                             verification */
  const char* tls_capath; /* use custom X509 certificates found by scanning this
                             directory  */

  const char* app_timeout;   /* application inactivity timeout */
  const char* start_timeout; /* timeout for acquiring a socket */

  /*
   * The following options control additional diagnostic and testing
   * behaviors within the daemon. Use with caution, the extra logging
   * and/or diagnostics may have high overhead.
   */
  int integration_mode; /* extra logging of transaction data for testing */
  int debug_http;       /* extra logging of communication with New Relic  */

  /*
   * The following struct contains flags that control data gathering for
   * utilization (Cloud-friendly pricing), and are passed using the `--define`
   * daemon argument.
   */
  nr_utilization_t utilization;
} nr_daemon_args_t;

/*
 * Purpose : Start a daemon process.
 *
 * Params  : 1. name of the daemon executable as expected by execve(2)
 *           2. arguments to pass to the daemon
 *
 * Returns : The process id of the daemon or -1 on error.
 */
extern pid_t nr_spawn_daemon(const char* path, const nr_daemon_args_t* args);

#endif /* NR_DAEMON_HDR */
