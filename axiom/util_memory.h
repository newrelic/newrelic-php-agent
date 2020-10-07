/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions which abstract system memory allocation
 * functions.
 *
 * This file follows the same conventions as util_strings.h for making nr_foo
 * shim wrappers for C library functions foo. See the discussion there.
 */
#ifndef UTIL_MEMORY_HDR
#define UTIL_MEMORY_HDR

#include "nr_axiom.h"

#if HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

#include <string.h>

/*
 * The macro nr_alloca must be a macro, as it must expand to an invocation of
 * the special function "alloca".
 */
#if defined(__GNUC__)
#define nr_alloca(X)                    \
  ({                                    \
    char* _alloca_p = (char*)alloca(X); \
    _alloca_p[0] = 0;                   \
    _alloca_p;                          \
  })
#else
#define nr_alloca(X) alloca(X)
#endif

/*
 * Semantics: free (NULL) always safe. Sets freed pointer to 0.
 * Due to strict aliasing issues this function is intentionally not inlined.
 */
extern void nr_realfree(void** oldptr);

/*
 * Semantics: malloc(0) always succeeds. Memory is not initialized to 0.
 */
extern void* nr_malloc(size_t size) NRMALLOC NRMALLOCSZ(1);

/*
 * Semantics: malloc(0) always succeeds. Memory IS initialized to 0.
 */
extern void* nr_zalloc(size_t size) NRMALLOC NRMALLOCSZ(1);

/*
 * Semantics: calloc (x,0) or calloc (0, x) always succeeds. Memory is
 * initialized to 0.
 */
extern void* nr_calloc(size_t numelt, size_t eltsize) NRMALLOC NRCALLOCSZ(1, 2);

/*
 * Semantics: realloc (NULL, x) always succeeds. realloc (x,0) always succeeds.
 */
extern void* nr_realloc(void* oldptr, size_t newsize) NRMALLOCSZ(2);

extern void* nr_reallocarray(void* ptr, size_t nmemb, size_t size) NRMALLOC
    NRCALLOCSZ(2, 3);

/*
 * Semantics: strdup (NULL) always succeeds.
 */
extern char* nr_strdup(const char* orig) NRMALLOC;

/*
 * Semantics:  strdup (NULL) always succeeds.
 * Given two string pointers, each possibly NULL, return a duplicate of
 * one, where the first pointer takes precedence over the second.  If
 * both parameters are NULL, mimic strdup semantics and return "".
 */
extern char* nr_strdup_or(const char* string_if_not_null,
                          const char* default_string) NRMALLOC;

/*
 * Semantics: strndup (NULL, x) always succeeds. Copies at most LEN bytes and
 * the returned pointer is always NUL terminated. Maximum length of the returned
 * pointer is LEN+1 bytes. If the string is less than LEN bytes it will be
 * allocated to be just big enough to hold the string. strndup (x, 0) always
 * a valid empty allocated string.
 */
extern char* nr_strndup(const char* orig, size_t len) NRMALLOC;

/*
 * Semantics: memset (NULL, x, y) always safe.
 */
static inline void* nr_memset(void* buf, int val, size_t count) {
  if (nrlikely(buf && (count > 0))) {
    return memset(buf, val, count);
  }
  return buf;
}

/*
 * Semantics: memcpy (NULL, x, y) always safe
 *            memcpy (x, NULL, y) always safe
 *            memcpy (x, y, 0) always safe
 *            Overlapping copies not guaranteed to work.
 */
static inline void* nr_memcpy(void* dest, const void* src, size_t len) {
  if (nrlikely(dest && src && (len > 0))) {
    return memcpy(dest, src, len);
  }
  return dest;
}

/*
 * Semantics: memmove (NULL, x, y) always safe
 *            memmove (x, NULL, y) always safe
 *            memmove (x, y, 0) always safe and does not work
 *            Overlapping copies guaranteed to work.
 */
static inline void* nr_memmove(void* dest, const void* src, size_t len) {
  if (nrlikely(dest && src && (len > 0))) {
    return memmove(dest, src, len);
  }
  return dest;
}

/*
 * Semantics: memcmp (anything, anything, 0) always returns 0
 *            memcmp (NULL, nonnull, X) always returns -1
 *            memcmp (nonnull, NULL, X) always returns 1
 *            NULL for either argument always OK.
 */
static inline int nr_memcmp(const void* s1, const void* s2, size_t len) {
  if (nrlikely(s1 && s2 && (len > 0))) {
    return memcmp(s1, s2, len);
  } else if (len <= 0) {
    return 0;
  }

  /* One or both pointers must be NULL. */
  return (NULL != s1) - (NULL != s2);
}

/*
 * Semantics: memchr (anything, anything, 0) always returns 0
 *            memchr (NULL, anything, X) always returns 0
 *            NULL for either argument always OK.
 */
static inline void* nr_memchr(const void* str, int c, size_t len) {
  if (nrlikely(str && (len > 0))) {
    return memchr(str, c, len);
  }
  return NULL;
}

#define nr_free(X) nr_realfree((void**)(&(X)))

#endif /* UTIL_MEMORY_HDR */
