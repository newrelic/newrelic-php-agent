/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Implementation of memory utility functions.
 */
#include "nr_axiom.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"

#include <sys/types.h>
#include <errno.h>

#undef free

void nr_realfree(void** oldptr) {
  if ((0 == oldptr) || (0 == *oldptr)) {
    return;
  }
  (free)(*oldptr);
  *oldptr = 0;
}

void* NRMALLOC NRMALLOCSZ(1) nr_malloc(size_t size) {
  void* ret;

  if (nrunlikely(size <= 0)) {
    size = 8;
  }

  ret = (malloc)(size);
  if (nrunlikely(0 == ret)) {
    nrl_error(NRL_MEMORY, "failed to allocate %zu byte(s)", size);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  return ret;
}

void* NRMALLOC NRMALLOCSZ(1) nr_zalloc(size_t size) {
  void* ret;

  if (nrunlikely(size == 0)) {
    size = 8;
  }

  ret = (calloc)(1, size);
  if (nrunlikely(0 == ret)) {
    nrl_error(NRL_MEMORY, "failed to allocate %zu byte(s)", size);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  return ret;
}

void* NRMALLOC NRCALLOCSZ(1, 2) nr_calloc(size_t nelem, size_t elsize) {
  void* ret;

  if (nrunlikely(nelem == 0)) {
    nelem = 1;
  }

  if (nrunlikely(elsize == 0)) {
    elsize = 1;
  }

  ret = (calloc)(nelem, elsize);
  if (nrunlikely(0 == ret)) {
    nrl_error(NRL_MEMORY, "failed to allocate %zu x %zu bytes", nelem, elsize);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  return ret;
}

void* NRMALLOCSZ(2) nr_realloc(void* oldptr, size_t newsize) {
  void* ret;

  if (nrunlikely(0 == oldptr)) {
    return nr_malloc(newsize);
  }

  if (nrunlikely(0 == newsize)) {
    newsize = 8;
  }

  ret = (realloc)(oldptr, newsize);
  if (nrunlikely(0 == ret)) {
    nrl_error(NRL_MEMORY, "failed to reallocate %p for %zu bytes", oldptr,
              newsize);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  return ret;
}

void* NRMALLOC NRCALLOCSZ(2, 3)
    nr_reallocarray(void* ptr, size_t nmemb, size_t size) {
  /*
   * This implementation is adapted from OpenSSH's OpenBSD compatibility layer:
   * https://github.com/openssh/openssh-portable/blob/285310b897969a63ef224d39e7cc2b7316d86940/openbsd-compat/reallocarray.c#L39-L44
   */
#define MUL_NO_OVERFLOW ((size_t)1 << (sizeof(size_t) * 4))

  if (nrunlikely((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW)
                 && nmemb > 0 && SIZE_MAX / nmemb < size)) {
    errno = ENOMEM;
    nrl_error(NRL_MEMORY,
              "reallocating %zu x %zu bytes would result in overflow", nmemb,
              size);
    return NULL;
  }

  /*
   * In general, the reference implementation has the same behaviour as
   * realloc() on all of our supported platforms, with one exception: realloc()
   * with a non-NULL pointer and a zero length size is equivalent to free() on
   * glibc, but not on all platforms.  For consistency, we'll use the glibc
   * behaviour.
   */
  if (nrunlikely(NULL != ptr && (0 == size || 0 == nmemb))) {
    (free)(ptr);
    return NULL;
  }

  return (realloc)(ptr, size * nmemb);
}

char* NRMALLOC nr_strdup(const char* orig) {
  char* ret;
  int lg;

  if (NULL == orig) {
    orig = "";
  }

  /*
   * malloc and strcpy are used here in place of strdup because gcc 4.9.1
   * address and thread sanitizer have issues with strdup in the context of PHP.
   * Quickly browsing the changelogs shows that the sanitizers sometimes treat
   * strdup specially.
   */
  lg = (strlen)(orig);
  ret = (char*)(malloc)(lg + 1);

  if (NULL == ret) {
    nrl_error(NRL_MEMORY | NRL_STRING, "failed to duplicate string %p", orig);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  (strcpy)(ret, orig);

  return ret;
}

char* NRMALLOC nr_strdup_or(const char* string_if_not_null,
                            const char* default_string) {
  if (NULL != string_if_not_null) {
    return nr_strdup(string_if_not_null);
  }
  if (NULL != default_string) {
    return nr_strdup(default_string);
  }
  return nr_strdup("");
}

char* NRMALLOC nr_strndup(const char* orig, size_t len) {
  char* ret;
  int olen;
  const char* np;

  if (nrunlikely(len == 0)) {
    return nr_strdup("");
  }

  if (nrunlikely(0 == orig)) {
    return nr_strdup("");
  }

  np = (const char*)(memchr)(orig, 0, len);
  if (nrlikely(0 != np)) {
    olen = np - orig;
  } else {
    olen = len;
  }

  ret = (char*)nr_malloc(olen + 1);

  if (nrunlikely(0 == ret)) {
    nrl_error(NRL_MEMORY | NRL_STRING, "failed to duplicate string %p %zu",
              orig, len);
    nr_signal_tracer_common(31); /* SIGSYS(31) - bad system call */
    exit(3);
  }

  (memcpy)(ret, orig, olen);
  ret[olen] = 0;

  return ret;
}

#define free free_notimplemented
