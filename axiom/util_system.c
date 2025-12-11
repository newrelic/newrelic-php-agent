/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <netinet/in.h>
#include <sys/utsname.h>

#if defined(__GLIBC__)
#include <gnu/libc-version.h>
#endif

#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_system.h"
#include "util_syscalls.h"

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

/* Populates the nr_system_t struct with libc info if available. */
static void nr_system_get_system_libc(nr_system_t* sys) {
  const char* libc_version = NULL;

  if (NULL == sys) {
    return;
  }

#if defined(__GLIBC__)
  libc_version = gnu_get_libc_version();
#endif
  /* NOTE: Currently unable to extract MUSL version. */

  if (nr_strempty(libc_version)) {
    sys->libc_version = nr_formatf("%s", LIBC_NAME);
  } else {
    sys->libc_version = nr_formatf("%s %s", LIBC_NAME, libc_version);
  }
}

/* Populates the nr_system_t struct with what we can get from etc/os-release. */
void nr_system_get_system_info_from_osrelease(nr_system_t* sys,
                                              char* osrelease_fname) {
  FILE* fd = NULL;
  char line[256];
  int len = 0;
  char* value = NULL;

#define REMOVE_LEADING_WHITESPACE                                        \
  value = line;                                                          \
  while (value && '\0' != *value && nr_isspace((unsigned char)*value)) { \
    value++;                                                             \
  }

#define STRIP_LINUX_NEWLINE               \
  if (len > 0 && line[len - 1] == '\n') { \
    line[len - 1] = '\0';                 \
    len--;                                \
  }

#define HANDLE_END_QUOTE                                            \
  if (len > 0 && ('"' == line[len - 1] || '\'' == line[len - 1])) { \
    line[len - 1] = '\0';                                           \
    len--;                                                          \
  }

#define HANDLE_START_QUOTE                   \
  if ('"' == value[0] || '\'' == value[0]) { \
    value += 1;                              \
  }

  if (NULL == osrelease_fname) {
    return;
  }

  if (NULL == sys) {
    return;
  }

  /* Check if the file exists. */
  if (0 != nr_access(osrelease_fname, F_OK)) {
    return;
  }

  /* Open file. */
  fd = fopen(osrelease_fname, "r");
  if (NULL == fd) {
    return;
  }

  while (fgets(line, sizeof(line), fd) != NULL) {
    REMOVE_LEADING_WHITESPACE;
    if (0 == nr_strncmp(value, VERSION_ID_STRING, VERSION_ID_STRING_LEN)) {
      len = strlen(line);
      STRIP_LINUX_NEWLINE;
      value = value + VERSION_ID_STRING_LEN;
      /*
       * Some OS have quoted VERSION_IDs so just remove quotes if they exist.
       */
      HANDLE_END_QUOTE;
      HANDLE_START_QUOTE;
      if ('\0' != *value) {
        sys->distro_version_id = nr_strdup(value);
      }
    } else if (0 == nr_strncmp(value, ID_STRING, ID_STRING_LEN)) {
      /*
       * Some OS have quoted IDs so just remove quotes if they exist.
       */
      len = strlen(line);
      STRIP_LINUX_NEWLINE;
      value = value + ID_STRING_LEN;
      HANDLE_END_QUOTE;
      HANDLE_START_QUOTE;
      if ('\0' != *value) {
        sys->distro_id = nr_strdup(value);
      }
    }
  }
  fclose(fd);
}

/* Populates the nr_system_t struct with what we can get from uname. */
static void nr_system_get_system_info_from_uname(nr_system_t* sys) {
  struct utsname uni;

  if (NULL == sys) {
    return;
  }

  nr_memset(&uni, 0, sizeof(uni));

  /*
   * POSIX requires uname() to return a non-negative integer on success.
   */
  if (uname(&uni) < 0) {
    return;
  }

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
}

nr_system_t* nr_system_get_system_information(void) {
  nr_system_t* sys;

  sys = (nr_system_t*)nr_zalloc(sizeof(nr_system_t));

  nr_system_get_system_info_from_uname(sys);
  nr_system_get_system_info_from_osrelease(sys, OSRELEASE_DIR_PATH);
  nr_system_get_system_libc(sys);
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
  nr_free(sys->distro_id);
  nr_free(sys->distro_version_id);
  nr_free(sys->libc_version);

  nr_realfree((void**)sys_ptr);
}

int nr_system_num_cpus(void) {
  return (int)sysconf(_SC_NPROCESSORS_ONLN);
}
