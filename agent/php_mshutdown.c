/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles the termination that happens once per module.
 */
#include "php_agent.h"

#include <signal.h>
#include <sys/wait.h>

#include "php_globals.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_vm.h"
#include "nr_agent.h"
#include "util_logging.h"
#include "fw_wordpress.h"

#ifdef TAGS
void zm_shutdown_newrelic(void); /* ctags landing pad only */
#endif
PHP_MSHUTDOWN_FUNCTION(newrelic) {
  (void)type;
  (void)module_number;
  NR_UNUSED_TSRMLS;

  if (0 == NR_PHP_PROCESS_GLOBALS(enabled)) {
    return SUCCESS;
  }

  /*
   * Note: When shutting down, each PHP process will perform this MSHUTDOWN
   * regardless of whether or not it performed a MINIT:  For example, in the
   * Apache worker situation, every PHP worker process will run this function,
   * not just the parent master process.  Therefore, this function must not
   * do any work that should only be completed by a single web server process.
   *
   * It is assumed that this MSHUTDOWN is only done once per PHP process.
   */
  nrl_debug(NRL_INIT, "MSHUTDOWN processing started");

  nr_wordpress_mshutdown();

  /* restore header handler */
  sapi_module.header_handler = NR_PHP_PROCESS_GLOBALS(orig_header_handler);
  NR_PHP_PROCESS_GLOBALS(orig_header_handler) = NULL;

  nr_agent_close_daemon_connection();

  nrl_close_log_file();

  nr_php_remove_opcode_handlers();
  nr_php_destroy_internal_wrap_records();
  nr_php_destroy_user_wrap_records();
  nr_php_global_destroy();
  nr_applist_destroy(&nr_agent_applist);

  return SUCCESS;
}
