/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for gathering system information.
 */
#ifndef UTIL_SYSTEM
#define UTIL_SYSTEM

#define VERSION_ID_STRING "VERSION_ID="
#define ID_STRING "ID="
#define VERSION_ID_STRING_LEN (sizeof(VERSION_ID_STRING)-1)
#define ID_STRING_LEN (sizeof(ID_STRING)-1)
#if defined(__GLIBC__)
#define LIBC_NAME "GLIBC"
#else
#define LIBC_NAME "MUSL"
#endif
#define OSRELEASE_DIR_PATH "/etc/os-release"

typedef struct _nr_system_t {
  /* Gathered from uname */
  char* sysname;  /* System name */
  char* nodename; /* Node name */
  char* release;  /* OS release */
  char* version;  /* OS version */
  char* machine;  /* Machine type */

  /*
   * Gathered from /etc/os-release
   * https://man.archlinux.org/man/os-release.5.en#DESCRIPTION
   * We'll get OS ID and OS version ID since those will provice the distribution
   * and version number simply with no spaces or quotes and it is common across
   * all OSs we currently support
   */

  char*
      distro_id; /* OS ID lowercase string IDing the OS distro with no spaces*/
  char* distro_version_id; /* OS version ID version of the os distro */

  /* Gathered from libc*/
  char* libc_version;
} nr_system_t;

/*
 * Purpose : Gather information about the current system.
 *
 * Returns : A newly allocated and populated nr_system_t structure.  It is
 *           the responsibility of the caller to destroy this structure using
 *           nr_system_destroy. The structure will be populated
 *           with information from uname, /etc/os-release, and libc.
 */
extern nr_system_t* nr_system_get_system_information(void);

/*
 * Purpose : Gather distroname and distroversion from the given osrelease file.
 *
 * Params  : 1. A valid nr_system_t pointer
 *           2. The path to the osrelease file.
 *
 * Returns : The nr_system_t structure will be populated with information from
 *           the specified osrelease file.
 * Note: expects the osrelease file to have format specified here:
 * https://man.archlinux.org/man/os-release.5.en#DESCRIPTION
 */
extern void nr_system_get_system_info_from_osrelease(nr_system_t* sys,
                                                     char* osrelease_fname);

/*
 * Purpose : Destroy a system structure of the type returned by
 *           nr_system_get_system_information.
 */
extern void nr_system_destroy(nr_system_t** sys_ptr);

/*
 * Purpose : Get the service port.
 *
 * Returns : A newly allocated string.  It is the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_system_get_service_port(const char* service,
                                        const char* port_type);

/*
 * Purpose : Get the host name.
 *
 * Returns : A newly allocated string.  It is the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_system_get_hostname(void);

/*
 * Purpose : Returns the number of logical processors available.
 *
 * Returns : The number of logical processors available.
 */
extern int nr_system_num_cpus(void);

#endif /* UTIL_SYSTEM */
