/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <netinet/in.h>
#include <sys/utsname.h>

#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"

char* nr_system_get_service_port(const char* service, const char* port_type) {
  struct servent* serv_ptr;
  char* port = NULL;

  if ((serv_ptr = getservbyname(service, port_type))) {
    port = nr_formatf("%d", ntohs((uint16_t)serv_ptr->s_port));
  }

  return port;
}

char* nr_system_get_hostname(void) {
  char hn[512];

  nr_memset(&hn, 0, sizeof(hn));

  gethostname(hn, sizeof(hn));
  hn[sizeof(hn) - 1] = 0;

  return nr_strdup(hn);
}

nr_system_t* nr_system_get_system_information(void) {
  struct utsname uni;
  nr_system_t* sys;

  nr_memset(&uni, 0, sizeof(uni));

  /*
   * POSIX requires uname() to return a non-negative integer on success.
   */
  if (uname(&uni) < 0) {
    return 0;
  }

  sys = (nr_system_t*)nr_zalloc(sizeof(nr_system_t));

  sys->sysname = nr_strdup(uni.sysname);
  sys->nodename = nr_strdup(uni.nodename);
  sys->release = nr_strdup(uni.release);
  sys->version = nr_strdup(uni.version);
  sys->machine = nr_strdup(uni.machine);

  {
    char* colon = nr_strchr(sys->version, ':');

    if (colon) {
      *colon = 0;
    }
  }

  return sys;
}

void nr_system_destroy(nr_system_t** sys_ptr) {
  nr_system_t* sys;

  if (0 == sys_ptr) {
    return;
  }
  sys = *sys_ptr;
  if (0 == sys) {
    return;
  }

  nr_free(sys->sysname);
  nr_free(sys->nodename);
  nr_free(sys->release);
  nr_free(sys->version);
  nr_free(sys->machine);

  nr_realfree((void**)sys_ptr);
}

int nr_system_num_cpus(void) {
  return (int)sysconf(_SC_NPROCESSORS_ONLN);
}
