/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "nr_banner.h"
#include "nr_version.h"
#include "util_logging.h"
#include "util_syscalls.h"
#include "util_system.h"

void nr_banner(const char* daemon_address,
               nr_daemon_startup_mode_t daemon_launch_mode,
               const char* agent_specific_info) {
  const char* stype = 0;
  const char* startup = 0;
  char osinfo[512];
  char daemon[64];
  char process_information[256];

  daemon[0] = 0;
  if (daemon_address) {
    snprintf(daemon, sizeof(daemon), "daemon='%s' ", daemon_address);
  }

  process_information[0] = 0;
  snprintf(process_information, sizeof(process_information),
           " pid=%d ppid=%d uid=%d euid=%d gid=%d egid=%d", nr_getpid(),
           nr_getppid(), nr_getuid(), nr_geteuid(), nr_getgid(), nr_getegid());

#if defined(HAVE_BACKTRACE)
  stype = " backtrace=yes";
#else
  stype = " backtrace=no";
#endif

  {
    nr_system_t* sys = nr_system_get_system_information();

    osinfo[0] = 0;

    if (sys) {
      snprintf(osinfo, sizeof(osinfo),
               " os='%s' rel='%s' mach='%s' ver='%s' node='%s'",
               NRBLANKSTR(sys->sysname), NRBLANKSTR(sys->release),
               NRBLANKSTR(sys->machine), NRBLANKSTR(sys->version),
               NRBLANKSTR(sys->nodename));
    }

    nr_system_destroy(&sys);
  }

  switch (daemon_launch_mode) {
    case NR_DAEMON_STARTUP_UNKOWN:
      startup = "";
      break;
    case NR_DAEMON_STARTUP_INIT:
      startup = " startup=init";
      break;
    case NR_DAEMON_STARTUP_AGENT:
      startup = " startup=agent";
      break;
    default:
      startup = "";
      break;
  }

  nrl_info(
      NRL_INIT,
      "New Relic %s"
      " [" NRP_FMT_UQ NRP_FMT_UQ NRP_FMT_UQ NRP_FMT_UQ NRP_FMT_UQ NRP_FMT_UQ
      "]",

      nr_version_verbose(),

      NRP_BUFFER(daemon),

      NRP_CONFIG((agent_specific_info ? agent_specific_info : "")),
      NRP_BUFFER(process_information),

      NRP_CONFIG(stype), NRP_CONFIG(startup), NRP_BUFFER(osinfo));
}
