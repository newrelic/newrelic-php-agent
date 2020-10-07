/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains a function to write agent/daemon info to the log file.
 */
#ifndef NR_BANNER_HDR
#define NR_BANNER_HDR

typedef enum _nr_daemon_startup_mode_t {
  NR_DAEMON_STARTUP_UNKOWN = -1, /* unknown startup mode */
  NR_DAEMON_STARTUP_INIT = 0,    /* daemon started up elsewhere */
  NR_DAEMON_STARTUP_AGENT = 1, /* daemon started up from the agent by forking */
} nr_daemon_startup_mode_t;

/*
 * Purpose : Emit a banner containing the agent version and other pertinent
 *           information, usually to the log file.
 *
 * Params  : 1. The daemon's location (e.g., port or udspath), if any.
 *           2. The daemon startup mode.
 *           3. A string containing extra information about the language or
 *              environment that axiom is supporting.  For PHP, this is the
 *              PHP version, the SAPI version, the zts flavor, and Apache
 *              information.  This string is optional.
 *
 * Returns : Nothing.
 *
 * Notes   : One of either workers or an external port must be specified, but
 *           not both (there is no such daemon mode that uses both workers and
 *           and external daemon).
 */
extern void nr_banner(const char* daemon_location,
                      nr_daemon_startup_mode_t daemon_launch_mode,
                      const char* agent_specific_info);

#endif /* NR_BANNER_HDR */
