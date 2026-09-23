/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_agent.h"
#include "util_threads.h"

/* This is defined only to satisfy link requirements, and is not shared amongst
 * threads. */

nrt_thread_local int nr_agent_daemon_fd = -1;

void nr_set_daemon_fd(int fd) {
  nr_agent_daemon_fd = fd;
}

int nr_agent_get_daemon_fd_locked(void) {
  return nr_agent_daemon_fd;
}

void nr_agent_close_daemon_connection(void) {}

nr_status_t nr_agent_lock_daemon_mutex(void) {
  return NR_SUCCESS;
}

nr_status_t nr_agent_unlock_daemon_mutex(void) {
  return NR_SUCCESS;
}
