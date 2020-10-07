/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This header includes basic compiler abstraction macros, common data types
 * and common system include files used by almost every file here.
 */
#ifndef NR_AXIOM_HDR
#define NR_AXIOM_HDR

#if !defined(_GNU_SOURCE)
/*
 * Required for the use of vasprintf.
 */
#define _GNU_SOURCE
#endif

#if !defined(__USE_UNIX98)
#define __USE_UNIX98
#endif

/*
 * Common return values from most functions
 *
 * Note that this follows the return value semantics for unix system calls,
 * namely == 0 is success, and < 0 is failure.
 */
typedef enum _nr_status_t {
  NR_SUCCESS = 0,
  NR_FAILURE = -1,
} nr_status_t;

/*
 * Compiler abstraction. If we are using GNU CC, we recommend using at least
 * version 4.7.3.
 *
 * Note that clang 3.5 (17Sep2014) defines __GNUC__,
 * but no longer implements __alloc_size__.
 */
#if defined(__GNUC__) /* { */

#define NRPURE __attribute__((pure))
#define NRUNUSED __attribute__((__unused__))
#define NRPRINTFMT(x) __attribute__((__format__(__printf__, x, x + 1)))
#define nrlikely(X) __builtin_expect(((X) != 0), 1)
#define nrunlikely(X) __builtin_expect(((X) != 0), 0)
#define NRMALLOC __attribute__((__malloc__))

#if defined(__clang__)
#define NRMALLOCSZ(X)
#define NRCALLOCSZ(X, Y)
#else
#define NRMALLOCSZ(X) __attribute__((__alloc_size__((X))))
#define NRCALLOCSZ(X, Y) __attribute__((__alloc_size__((X), (Y))))
#endif

#define NRNOINLINE __attribute__((__noinline__))
#ifndef NRINLINE
#define NRINLINE static __inline__
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
NRINLINE void* nr_remove_const(const void* ptr) {
  return (void*)ptr;
}
#pragma GCC diagnostic pop

#else /* } { */

#define NRPURE        /**/
#define NRUNUSED      /**/
#define NRPRINTFMT(x) /**/
#define nrlikely(X) X
#define nrunlikely(X) X
#define NRMALLOC         /**/
#define NRMALLOCSZ(X)    /**/
#define NRCALLOCSZ(X, Y) /**/
#ifndef NRINLINE
#define NRINLINE static inline
#endif
#define NRNOINLINE /**/
#define nr_remove_const(X) X

#endif /* } */

/*
 * The macro nr_clang_assert is redefined to assert iff we are compiling using
 * the clang static analyzer scan-build.  scan-build is sensitive to
 * calls to the assert macro, and will prune its search space appropriately.
 * Because of the way that we preprocess our source files to present to
 * scan-build, this redefinition happens late in the preprocessing pipeline, so
 * that an invocation of the assert macro is not expanded prematurely.
 */
#if !defined(__clang_analyzer__)
#define nr_clang_assert(P)
#endif

#define NRSAFESTR(S) ((S) ? (S) : "<NULL>")
#define NRBLANKSTR(S) ((S) ? (S) : "")

/*
 * Safely (ish) converts size_t lengths into signed ints.
 */
#define NRSAFELEN(L) ((((int)L) < 0) ? 0 : ((int)L))

/*
 * macro magic needed to make quoted strings.
 */
#define NR_STR1(X) #X
#define NR_STR2(X) NR_STR1(X)

/*
 * More macro magic for calling functions with constant strings.
 */
#define NR_PSTR(S)                                                            \
  (S), sizeof(S)                                                              \
           - 1 /* a C string, with a length that EXCLUDES the null terminator \
                */
#define NR_HSTR(S)                                                   \
  (S), sizeof(S) /* a C string, with a length that INCLUDES the null \
                    terminator (used for php hash tables) */

/*
 * Basic format specifiers.
 */
#define NR_INT64_FMT "%" PRId64
#define NR_UINT64_FMT "%" PRIu64
#define NR_AGENT_RUN_ID_FMT "%s"

#endif /* NR_AXIOM_HDR */
