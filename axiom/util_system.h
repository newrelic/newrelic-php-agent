/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions for gathering system information.
 */
#ifndef UTIL_SYSTEM
#define UTIL_SYSTEM

typedef struct _nr_system_t {
  char* sysname;  /* System name */
  char* nodename; /* Node name */
  char* release;  /* OS release */
  char* version;  /* OS version */
  char* machine;  /* Machine type */
} nr_system_t;

/*
 * Purpose : Gather information about the current system.
 *
 * Returns : A newly allocated and populated nr_system_t structure.  It is
 *           the responsibility of the caller to destroy this structure using
 *           nr_system_destroy.
 */
extern nr_system_t* nr_system_get_system_information(void);

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
