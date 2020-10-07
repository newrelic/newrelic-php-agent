/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "nr_banner.h"
#include "php_agent.h"
#include "php_globals.h"
#include "php_newrelic.h"
#include "util_memory.h"
#include "util_syscalls.h"

static void test_nr_php_get_daemon_startup_mode(void) {
  nr_daemon_startup_mode_t daemon_startup_mode;

  /*
   * Test that no daemon starts if a manual configuration file exists. If the
   * /etc/newrelic/newrelic.cfg exists, the agent shouldn't start the daemon as
   * it assumes the daemon is meant to be started by its startup script.
   */
  if (0 == nr_access("/etc/newrelic/newrelic.cfg", F_OK)) {
    /*
     * If the file exists, check for NR_DAEMON_STARTUP_INIT and end this test
     * because none of the other conditions will be reached.
     */
    daemon_startup_mode = nr_php_get_daemon_startup_mode();
    tlib_pass_if_int_equal(
        "no daemon starts if a manual configuration file exists",
        NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

    /*
     * None of the other conditions in nr_php_get_daemon_startup_mode()
     * will be reached so no need to run the other test checks below.
     */
    return;
  }

  /*
   * Set global values to ensure NR_DAEMON_STARTUP_AGENT is returned.
   */
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 0;
  NR_PHP_PROCESS_GLOBALS(cli) = 0;

  /*
   * Test NULL case for daemon_conn_params. Daemon connection information will
   * be unknown so it won't be started by the agent.
   */
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "daemon connection info unknown. It won't be started by the agent",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)
      = (nr_conn_params_t*)nr_zalloc(sizeof(nr_conn_params_t));
  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)->type = NR_AGENT_CONN_UNKNOWN;

  /*
   * Test daemon started by agent mode.
   */
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal("daemon will be started by agent",
                         NR_DAEMON_STARTUP_AGENT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the command line version of PHP was used.
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 1;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 1;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if command line version of PHP was used",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if non-command line version of PHP was used (for
   * example Apache or php-fpm) then the agent will not start the daemon
   * (only the command line usage will start the daemon)
   */
  NR_PHP_PROCESS_GLOBALS(cli) = 0;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 2;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if non-command line version of PHP was used",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the daemon is configured via newrelic.cfg and
   * starting it outside of the agent.
   */
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 3;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if daemon is configured via newrelic.cfg and starting "
      "it outside of the agent.",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);

  /*
   * Test that no daemon starts if the daemon connection settings specify
   * a host different from the local host
   */
  NR_PHP_PROCESS_GLOBALS(daemon_conn_params)->type
      = NR_AGENT_CONN_TCP_HOST_PORT;
  NR_PHP_PROCESS_GLOBALS(no_daemon_launch) = 0;
  daemon_startup_mode = nr_php_get_daemon_startup_mode();
  tlib_pass_if_int_equal(
      "no daemon starts if the daemon connection settings specify a host "
      "different from the local host",
      NR_DAEMON_STARTUP_INIT, daemon_startup_mode);
  nr_conn_params_free(nr_php_per_process_globals.daemon_conn_params);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_nr_php_get_daemon_startup_mode();
}
