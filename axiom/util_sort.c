/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef NR_SYSTEM_LINUX
/*
 * To use qsort_r() on glibc, we need _GNU_SOURCE defined before including _any_
 * glibc headers, even though we'd really prefer to include features.h, feature
 * test, then set up _GNU_SOURCE. Oh well.
 *
 * For added fun, features.h doesn't exist on non-Linux OSes as a general rule.
 */
#define _GNU_SOURCE
#include <features.h>
#endif /* NR_SYSTEM_LINUX */

/*
 * Figure out what, if any, qsort_r() is available.
 *
 * glibc 2.8 and later have qsort_r() with arg as the last parameter. We can
 * use __GLIBC_PREREQ to figure out if that's the version we have.
 *
 * BSD libc generally has qsort_r() with arg as the first parameter. This is
 * available on all FreeBSD and macOS versions we support.
 *
 * Otherwise, we'll have to shim something ourselves using qsort() and thread
 * locals.
 *
 * At any rate, exactly one of HAVE_GLIBC_QSORT_R, HAVE_BSD_QSORT_R, and
 * HAVE_NO_QSORT_R will be defined after this mess of preprocessor directives.
 */
// clang-format off
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
# if __GLIBC_PREREQ(2, 8)
#  define HAVE_GLIBC_QSORT_R
# else
#  define HAVE_NO_QSORT_R
# endif
#elif defined(NR_SYSTEM_FREEBSD) || defined(NR_SYSTEM_DARWIN)
# define HAVE_BSD_QSORT_R
#else
# define HAVE_NO_QSORT_R
#endif
// clang-format on

#include <stdlib.h>

#include "util_sort.h"
#include "util_threads.h"

#ifndef HAVE_GLIBC_QSORT_R
typedef struct _nr_sort_wrapper_t {
  nr_sort_cmp_t compar;
  void* arg;
} nr_sort_wrapper_t;
#endif

#ifdef HAVE_BSD_QSORT_R
static int nr_sort_wrapper_bsd(void* userdata, const void* a, const void* b) {
  nr_sort_wrapper_t* wrap = (nr_sort_wrapper_t*)userdata;

  return (wrap->compar)(a, b, wrap->arg);
}
#endif

#ifdef HAVE_NO_QSORT_R
static nrt_thread_local nr_sort_wrapper_t nr_sort_wrapper_data;

static int nr_sort_wrapper_tls(const void* a, const void* b) {
  return (nr_sort_wrapper_data.compar)(a, b, nr_sort_wrapper_data.arg);
}
#endif

void nr_sort(void* base,
             size_t nmemb,
             size_t size,
             nr_sort_cmp_t compar,
             void* arg) {
  if (NULL == base || NULL == compar) {
    return;
  }

#if defined(HAVE_GLIBC_QSORT_R)
  qsort_r(base, nmemb, size, compar, arg);
#elif defined(HAVE_BSD_QSORT_R)
  {
    nr_sort_wrapper_t wrap = {.compar = compar, .arg = arg};

    qsort_r(base, nmemb, size, &wrap, nr_sort_wrapper_bsd);
  }
#else
  nr_sort_wrapper_data.compar = compar;
  nr_sort_wrapper_data.arg = arg;

  qsort(base, nmemb, size, nr_sort_wrapper_tls);
#endif
}
