/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_agent.h"
#include "nr_app.h"

/* This is defined only to satisfy link requirements, and is not shared amongst
 * threads. */
nrapplist_t* nr_agent_applist = 0;

void nr_agent_close_daemon_connection(void) {}

nr_status_t nr_agent_lock_daemon_mutex(void) {
  return NR_SUCCESS;
}

nr_status_t nr_agent_unlock_daemon_mutex(void) {
  return NR_SUCCESS;
}

int nr_get_daemon_fd(void) {
  return 0;
}

nrapp_t* nr_app_verify_id(nrapplist_t* applist NRUNUSED,
                          const char* agent_run_id NRUNUSED) {
  return 0;
}
