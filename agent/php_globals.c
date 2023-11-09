/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file handles global variables and state.
 */
#include "php_agent.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_threads.h"

/*
 * There are some agent initialization tasks that need to be performed after
 * all modules' MINIT functions have been called and the PHP VM is fully up
 * and running. This variable (protected by a mutex) detects that and calls
 * the late initialization function once per process.
 */
static int done_first_rinit_work = 0;
nrthread_mutex_t first_rinit_mutex = NRTHREAD_MUTEX_INITIALIZER;

nrphpglobals_t nr_php_per_process_globals;

static void nr_php_per_process_globals_dispose(void) {
  nr_free(nr_php_per_process_globals.collector);
  nr_free(nr_php_per_process_globals.proxy);
  nr_free(nr_php_per_process_globals.daemon);
  nr_free(nr_php_per_process_globals.pidfile);
  nr_free(nr_php_per_process_globals.daemon_logfile);
  nr_free(nr_php_per_process_globals.daemon_loglevel);
  nr_free(nr_php_per_process_globals.daemon_auditlog);
  nr_free(nr_php_per_process_globals.daemon_app_timeout);
  nr_free(nr_php_per_process_globals.daemon_start_timeout);
  nr_free(nr_php_per_process_globals.udspath);
  nr_free(nr_php_per_process_globals.address_path);
  nr_conn_params_free(nr_php_per_process_globals.daemon_conn_params);
  nr_free(nr_php_per_process_globals.php_version);
  nr_free(nr_php_per_process_globals.upgrade_license_key);
  nro_delete(nr_php_per_process_globals.appenv);
  nro_delete(nr_php_per_process_globals.metadata);
  nr_free(nr_php_per_process_globals.env_labels);
  nr_free(nr_php_per_process_globals.apache_add);
  nr_free(nr_php_per_process_globals.docker_id);

  nr_memset(&nr_php_per_process_globals, 0, sizeof(nr_php_per_process_globals));
}

static void nr_php_reset_first_rinit_complete(void) {
  nrt_mutex_lock(&first_rinit_mutex);
  done_first_rinit_work = 0;
  nrt_mutex_unlock(&first_rinit_mutex);
}

void nr_php_global_init(void) {
  nr_php_reset_first_rinit_complete();
  nr_memset(&nr_php_per_process_globals, 0, sizeof(nr_php_per_process_globals));
}

void nr_php_global_destroy(void) {
  nr_php_per_process_globals_dispose();
}

void nr_php_global_once(nr_php_global_once_func_t func) {
  if (0 == done_first_rinit_work) {
    nrt_mutex_lock(&first_rinit_mutex);
    {
      /*
       * Yes we check this again in case another thread snuck in and started
       * doing late initialization already.
       */
      if (0 == done_first_rinit_work) {
        (func)();
        done_first_rinit_work = 1;
      }
    }
    nrt_mutex_unlock(&first_rinit_mutex);
  }
}
