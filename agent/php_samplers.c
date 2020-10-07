/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_samplers.h"
#include "util_errno.h"
#include "util_logging.h"
#include "util_metrics.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_system.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#ifdef HAVE_MALLOC_MALLOC_H
#include <malloc/malloc.h>
#endif

#ifdef NR_SYSTEM_DARWIN
#include <sys/sysctl.h>
#endif

#ifdef NR_SYSTEM_SOLARIS
#ifndef _LP64
#undef _LARGEFILE_SOURCE
#undef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 32
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <procfs.h>
#endif

#define timeval_to_micros(T) \
  (int64_t)(((int64_t)(T).tv_sec * 1000000) + (int64_t)(T).tv_usec)

static int page_size = 4096; /* Common */

/*
 * Initialize the samplers. This will determine various bits of information
 * that never change, and are used by the actual periodic samplers.
 */
void nr_php_initialize_samplers(void) {
#ifdef HAVE_GETPAGESIZE
  page_size = getpagesize();
#else
  page_size = 4096; /* Default. */
#endif
}

/*
 * This function is used to get the amount of physical memory used, in bytes.
 * We have several different stategies for computing this depending on what
 * the platform supports. MacOS fills in the RSS field for getrusage() but
 * Linux doesn't. Solaris does but expresses the value as multiples of the
 * page size, whereas MacOS expresses it in kilobytes. Thus the order of
 * these pre-processor conditionals is fairly important. We go from the most
 * platform-specific to the least. The final case, which is if we have both
 * getrusage() and getpagesize() is the fallback case for all systems where
 * we don't have another specific strategy. If any of the methods fail or
 * if we can't determine the memory usage method, return 0.
 */
#if defined(NR_SYSTEM_LINUX) && defined(HAVE_PROC_SELF_STATM)
static int64_t get_physical_memory_used(void) {
  FILE* fp = fopen("/proc/self/statm", "r");
  char tmpbuf[1024];
  char* ptr = NULL;
  int64_t rv;

  if ((FILE*)0 == fp) {
    nrl_verbosedebug(NRL_MISC, "/proc/self open failed - memory reported as 0");
    return 0;
  }

  if ((char*)0 == fgets(tmpbuf, sizeof(tmpbuf), fp)) {
    nrl_verbosedebug(NRL_MISC, "/proc/self read failed - memory reported as 0");
    fclose(fp);
    return 0;
  }

  fclose(fp);

  tmpbuf[sizeof(tmpbuf) - 1] = 0;

  /*
   * Skip over the first number to get the RSS, but fall back to the full
   * program size if we didn't have more than 1 number reported.
   */
  ptr = nr_strchr(tmpbuf, ' ');
  if (ptr) {
    ptr++;
  } else {
    ptr = tmpbuf;
  }
  rv = (int64_t)strtoll(ptr, 0, 0);

  nrl_verbosedebug(NRL_MISC, "/proc/self read: %lld of %d-byte pages",
                   (long long int)rv, page_size);
  return (rv * page_size);
}
#elif defined(NR_SYSTEM_DARWIN)
static int64_t get_physical_memory_used(void) {
  struct rusage rusage;
  int ret = getrusage(RUSAGE_SELF, &rusage);

  if (-1 == ret) {
    int err = errno;
    nrl_verbosedebug(NRL_MISC, "getrusage() failed with %d (%.16s)", err,
                     nr_errno(err));
    return 0;
  }

  return ((int64_t)(rusage.ru_maxrss * 1024));
}
#elif defined(NR_SYSTEM_SOLARIS)
static int64_t get_physical_memory_used(void) {
  int pid = nr_getpid();
  char procfn[128];
  struct psinfo psi;
  int fd;
  int br;

  snprintf(procfn, sizeof(procfn), "/proc/%d/psinfo", (int)pid);
  fd = open(procfn, O_RDONLY);
  if (-1 == fd) {
    return 0;
  }

  br = (int)read(fd, &psi, sizeof(psi));
  (void)close(fd);

  if (br != (int)sizeof(psi)) {
    return 0;
  }

  return ((int64_t)psi.pr_rssize * 1024);
}
#elif defined(HAVE_GETRUSAGE) && defined(HAVE_GETPAGESIZE)
static int64_t get_physical_memory_used(void) {
  struct rusage rusage;
  int ret = getrusage(RUSAGE_SELF, &rusage);

  if (-1 == ret) {
    int err = errno;
    nrl_verbosedebug(NRL_MISC, "getrusage() failed with %d (%.16s)", err,
                     nr_errno(err));
    return 0;
  }

  return ((int64_t)(rusage.ru_maxrss * page_size));
}
#else
#ifndef BUILDING_DEPENDENCIES
#error "Need to know how to get memory usage"
#endif
#endif

void nr_php_resource_usage_sampler_start(TSRMLS_D) {
  int ret;
  nrtime_t now = nr_get_time();
  struct rusage rusage;

  ret = getrusage(RUSAGE_SELF, &rusage);
  if (-1 == ret) {
    int err = errno;
    nrl_verbosedebug(NRL_MISC, "getrusage() failed with %d (%.16s)", err,
                     nr_errno(err));
    NRPRG(start_sample) = 0;
    return;
  }

  NRPRG(start_sample) = now;
  NRPRG(start_user_time).tv_sec = rusage.ru_utime.tv_sec;
  NRPRG(start_user_time).tv_usec = rusage.ru_utime.tv_usec;
  NRPRG(start_sys_time).tv_sec = rusage.ru_stime.tv_sec;
  NRPRG(start_sys_time).tv_usec = rusage.ru_stime.tv_usec;
}

void nr_php_resource_usage_sampler_end(TSRMLS_D) {
  int ret;
  int num_cpus;
  int64_t mem_used = (get_physical_memory_used() * 1000000) / (1024 * 1024);
  nrtime_t now = nr_get_time();
  int64_t elapsed_time;
  int64_t start_sms;
  int64_t end_sms;
  int64_t start_ums;
  int64_t end_ums;
  int64_t start_total;
  int64_t end_total;
  int64_t diff;
  int64_t fraction_usage;
  struct rusage rusage;

  /*
   * A brief note on why we multiply the memory values. The New Relic Platform
   * expects all metrics to be in float fractional seconds (with millisecond
   * accuracy). However, our metric data structure, and the one used by the
   * daemon, uses 64-bit integers and stores things in microsecond accuracy.
   * When the daemon serializes metrics for transmission to the New Relic
   * Platform, it takes all metric values and divides them by 1000000 (eg, 1000
   * * 1000). Therefore, since we need these values to represent actual
   * megabytes of memory used, we need to scale them up by 1000 * 1000 here.
   */
#ifdef HAVE_MSTATS
  {
    struct mstats ms = mstats();

    nrm_force_add_ex(NRTXN(unscoped_metrics), "Memory/RSS", mem_used, 0);

    mem_used = (ms.bytes_total * (1000 * 1000)) / (1024 * 1024);
    /* Heap memory usage in megabytes. */
    nrm_force_add_ex(NRTXN(unscoped_metrics), "Memory/Physical", mem_used, 0);
  }
#else
  {
    /* Physical memory usage in megabytes. */
    nrm_force_add_ex(NRTXN(unscoped_metrics), "Memory/Physical", mem_used, 0);
  }
#endif

  if (nrunlikely(0 == NRPRG(start_sample))) {
    /* If getrusage failed during the start sampler. */
    return;
  }

  ret = getrusage(RUSAGE_SELF, &rusage);
  if (-1 == ret) {
    int err = errno;
    nrl_verbosedebug(NRL_MISC, "getrusage() failed with %d (%.16s)", err,
                     nr_errno(err));
    return;
  }

  elapsed_time = now - NRPRG(start_sample);
  if (nrunlikely(elapsed_time <= 0)) {
    nrl_verbosedebug(NRL_MISC,
                     "elapsed time is not positive - no CPU sampler data "
                     "available this cycle");
    return;
  }

  start_ums = timeval_to_micros(NRPRG(start_user_time));
  start_sms = timeval_to_micros(NRPRG(start_sys_time));
  start_total = start_ums + start_sms;

  end_ums = timeval_to_micros(rusage.ru_utime);
  end_sms = timeval_to_micros(rusage.ru_stime);
  end_total = end_ums + end_sms;

  diff = end_total - start_total;
  if (diff < 0) {
    nrl_verbosedebug(NRL_MISC,
                     "user time difference is negative - no CPU sampler data "
                     "available this cycle");
    return;
  }
  nrm_force_add_ex(NRTXN(unscoped_metrics), "CPU/User Time", diff, 0);

  /*
   * For the CPU utilization metric, The New Relic Platform expects it in the
   * range [0.0 .. 1.0].
   *
   * However, recall that the consumer of this metric will divide by
   * 1000 * 1000 in its blind assumption that all metrics are in usecs.
   * We thus need to upscale, and do so carefully to minimize truncation
   * due to integer division.
   */

  num_cpus = nr_system_num_cpus();
  if (num_cpus < 1) {
    num_cpus = 1;
  }

  /*
   * Upscale the value to compensate for later downscale.
   * Compute this as a ratio; we expect diff < (elapsed_time * num_cpus).
   *
   * Since we use integer division here, carefully associate and group
   * large terms in the numerator to minimize truncation errors.
   */
  fraction_usage = (1000 * 1000 * 1 * diff) / (elapsed_time * num_cpus);
  nrm_force_add_ex(NRTXN(unscoped_metrics), "CPU/User/Utilization",
                   fraction_usage, 0);
}
