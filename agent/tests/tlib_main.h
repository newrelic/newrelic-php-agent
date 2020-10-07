/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file is a unit test framework.
 */
#ifndef TLIB_MAIN_HDR
#define TLIB_MAIN_HDR

#include "nr_axiom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"
#include "util_time.h"

typedef struct _tlib_parallel_info_t {
  /*
   * The suggested number of threads to use.
   * If <  0 run sequentially with absolute-value number of iterations
   * If == 0 run once with 1 thread
   * If >  0 run in parallel with this many threads if invoked -j0
   */
  int suggested_nthreads;
  size_t state_size;
} tlib_parallel_info_t;

extern tlib_parallel_info_t parallel_info;

/*
 * Where to find the directory in axiom/tests holding the reference files.
 */
#if !defined(REFERENCE_DIR)
#define REFERENCE_DIR "reference/"
#endif

/*
 * Where to find the directory in axiom/tests holding the cross agent tests
 */
#if !defined(CROSS_AGENT_TESTS_DIR)
#define CROSS_AGENT_TESTS_DIR "cross_agent_tests/"
#endif

/*
 * This macro is assigned to pointers in order to test that they are not
 * dereferenced.
 */
#define TLIB_BAD_PTR (void*)3

/*
 * The argc given to main().
 */
extern int tlib_argc;

/*
 * The argv given to main().
 */
extern char* const* tlib_argv;

/*
 * Number of tests passed.
 */
extern int tlib_passcount;

/*
 * Number of tests failed.
 */
extern int tlib_unexpected_failcount;

/*
 * Purpose: Change tlib_passcount under lock.
 */
extern int tlib_did_pass(void);

/*
 * Purpose: Change tlib_unexpected_failcount under lock.
 */
extern int tlib_did_fail(void);

/*
 * Check if file does / does not exist.
 */
extern int tlib_pass_if_exists_f(const char* file, const char* f, int line);
extern int tlib_pass_if_not_exists_f(const char* file, const char* f, int line);

/*
 * Check if condition true or false.
 */
extern int tlib_pass_if_true_f(const char* what,
                               int val,
                               const char* file,
                               int line,
                               const char* cond,
                               const char* fmt,
                               ...) NRPRINTFMT(6);
extern int tlib_pass_if_false_f(const char* what,
                                int val,
                                const char* file,
                                int line,
                                const char* cond,
                                const char* fmt,
                                ...) NRPRINTFMT(6);
extern int tlib_fail_if_true_f(const char* what,
                               int val,
                               const char* file,
                               int line,
                               const char* cond,
                               const char* fmt,
                               ...) NRPRINTFMT(6);
extern int tlib_fail_if_false_f(const char* what,
                                int val,
                                const char* file,
                                int line,
                                const char* cond,
                                const char* fmt,
                                ...) NRPRINTFMT(6);

/*
 * Execute a command. Pass if it exits normally, fail (and show output) if
 * it fails. This is intended for running diff -u against expected output
 * files. If you don't want to show the banner on failure set notdiff to 1.
 */
extern nr_status_t tlib_pass_if_exec_f(const char* what,
                                       const char* cmd,
                                       int notdiff,
                                       const char* file,
                                       int line);
extern nr_status_t tlib_pass_if_not_diff_f(const char* result_file,
                                           const char* expect_file,
                                           const char* transformation,
                                           int do_sort,
                                           int not_diff,
                                           const char* file,
                                           int line);

#define tlib_pass_if_exists(F) tlib_pass_if_exists_f((F), __FILE__, __LINE__)
#define tlib_pass_if_not_exists(F) \
  tlib_pass_if_not_exists_f((F), __FILE__, __LINE__)
#define tlib_pass_if_true(M, T, ...) \
  tlib_pass_if_true_f((M), (T), __FILE__, __LINE__, #T, __VA_ARGS__)
#define tlib_pass_if_false(M, T, ...) \
  tlib_pass_if_false_f((M), (T), __FILE__, __LINE__, #T, __VA_ARGS__)
#define tlib_fail_if_true(M, T, ...) \
  tlib_fail_if_true_f((M), (T), __FILE__, __LINE__, #T, __VA_ARGS__)
#define tlib_fail_if_false(M, T, ...) \
  tlib_fail_if_false_f((M), (T), __FILE__, __LINE__, #T, __VA_ARGS__)
#define tlib_pass_if_exec(W, C, N) \
  tlib_pass_if_exec_f((W), (C), (N), __FILE__, __LINE__)
#define tlib_pass_if_not_diff(R, E, T, S, D) \
  tlib_pass_if_not_diff_f((R), (E), (T), (S), (D), __FILE__, __LINE__)

/*
 * Shortcut macros for basic equality tests.
 */
#define tlib_pass_if_equal(M, EXPECTED, ACTUAL, TYPE, FORMAT)                  \
  {                                                                            \
    TYPE tlib_test_actual = (ACTUAL);                                          \
    TYPE tlib_test_expected = (EXPECTED);                                      \
                                                                               \
    tlib_pass_if_true_f((M), tlib_test_expected == tlib_test_actual, __FILE__, \
                        __LINE__, #EXPECTED " == " #ACTUAL,                    \
                        #EXPECTED "=" FORMAT " " #ACTUAL "=" FORMAT,           \
                        tlib_test_expected, tlib_test_actual);                 \
  }
#define tlib_fail_if_equal(M, EXPECTED, ACTUAL, TYPE, FORMAT)          \
  {                                                                    \
    TYPE tlib_test_actual = (ACTUAL);                                  \
    TYPE tlib_test_expected = (EXPECTED);                              \
                                                                       \
    tlib_pass_if_false_f((M), tlib_test_expected == tlib_test_actual,  \
                         __FILE__, __LINE__, #EXPECTED " == " #ACTUAL, \
                         #EXPECTED "=" FORMAT " " #ACTUAL "=" FORMAT,  \
                         tlib_test_expected, tlib_test_actual);        \
  }

/*
 * Even more shortcut macros for lazy developers, covering bool, char, int,
 * long, int64_t, the unsigned versions of the aforementioned, and double.
 */
#define tlib_pass_if_bool_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), bool, "%d")
#define tlib_fail_if_bool_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), bool, "%d")
#define tlib_pass_if_char_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), char, "%c")
#define tlib_fail_if_char_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), char, "%c")
#define tlib_pass_if_uchar_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), unsigned char, "%c")
#define tlib_fail_if_uchar_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), unsigned char, "%c")
#define tlib_pass_if_int_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), int, "%d")
#define tlib_fail_if_int_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), int, "%d")
#define tlib_pass_if_uint_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), unsigned int, "%u")
#define tlib_fail_if_uint_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), unsigned int, "%u")
#define tlib_pass_if_long_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), long, "%ld")
#define tlib_fail_if_long_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), long, "%ld")
#define tlib_pass_if_ulong_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), unsigned long, "%lu")
#define tlib_fail_if_ulong_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), unsigned long, "%lu")
#define tlib_pass_if_int8_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), int8_t, "%" PRId8)
#define tlib_fail_if_int8_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), int8_t, "%" PRId8)
#define tlib_pass_if_uint8_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), uint8_t, "%" PRIu8)
#define tlib_fail_if_uint8_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), uint8_t, "%" PRIu8)
#define tlib_pass_if_int16_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), int16_t, "%" PRId16)
#define tlib_fail_if_int16_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), int16_t, "%" PRId16)
#define tlib_pass_if_uint16_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), uint16_t, "%" PRIu16)
#define tlib_fail_if_uint16_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), uint16_t, "%" PRIu16)
#define tlib_pass_if_int32_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), int32_t, "%" PRId32)
#define tlib_fail_if_int32_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), int32_t, "%" PRId32)
#define tlib_pass_if_uint32_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), uint32_t, "%" PRIu32)
#define tlib_fail_if_uint32_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), uint32_t, "%" PRIu32)
#define tlib_pass_if_int64_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), int64_t, "%" PRId64)
#define tlib_fail_if_int64_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), int64_t, "%" PRId64)
#define tlib_pass_if_uint64_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), uint64_t, "%" PRIu64)
#define tlib_fail_if_uint64_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), uint64_t, "%" PRIu64)
#define tlib_pass_if_intptr_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), intptr_t, "%" PRIdPTR)
#define tlib_fail_if_intptr_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), intptr_t, "%" PRIdPTR)
#define tlib_pass_if_uintptr_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), uintptr_t, "%" PRIuPTR)
#define tlib_fail_if_uintptr_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), uintptr_t, "%" PRIuPTR)
#define tlib_pass_if_size_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), size_t, "%zu")
#define tlib_fail_if_size_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), size_t, "%zu")
#define tlib_pass_if_ssize_t_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), ssize_t, "%zd")
#define tlib_fail_if_ssize_t_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), ssize_t, "%zd")
#define tlib_pass_if_double_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), double, "%f")
#define tlib_fail_if_double_equal(M, EXPECTED, ACTUAL) \
  tlib_fail_if_equal((M), (EXPECTED), (ACTUAL), double, "%f")

#define tlib_pass_if_time_equal(M, EXPECTED, ACTUAL) \
  tlib_pass_if_equal((M), (EXPECTED), (ACTUAL), nrtime_t, NR_TIME_FMT)

/*
 * Shortcut equality tests for nr_status_t.
 */
extern int tlib_pass_if_status_success_f(const char* what,
                                         nr_status_t val,
                                         const char* file,
                                         int line,
                                         const char* cond);

extern int tlib_fail_if_status_success_f(const char* what,
                                         nr_status_t val,
                                         const char* file,
                                         int line,
                                         const char* cond);

#define tlib_pass_if_status_success(M, T) \
  tlib_pass_if_status_success_f((M), (T), __FILE__, __LINE__, #T)
#define tlib_fail_if_status_success(M, T) \
  tlib_fail_if_status_success_f((M), (T), __FILE__, __LINE__, #T)
#define tlib_pass_if_status_failure(M, T) \
  tlib_fail_if_status_success_f((M), (T), __FILE__, __LINE__, #T)
#define tlib_fail_if_status_failure(M, T) \
  tlib_pass_if_status_success_f((M), (T), __FILE__, __LINE__, #T)

/*
 * String versions of the above equality tests.
 */
#define tlib_pass_if_str_equal(M, EXPECTED, ACTUAL)                        \
  tlib_check_if_str_equal_f((M), #EXPECTED, (EXPECTED), #ACTUAL, (ACTUAL), \
                            true, __FILE__, __LINE__)

#define tlib_fail_if_str_equal(M, EXPECTED, ACTUAL)                        \
  tlib_check_if_str_equal_f((M), #EXPECTED, (EXPECTED), #ACTUAL, (ACTUAL), \
                            false, __FILE__, __LINE__)

static inline void tlib_check_if_str_equal_f(const char* what,
                                             const char* expected_literal,
                                             const char* expected,
                                             const char* actual_literal,
                                             const char* actual,
                                             bool expect_match,
                                             const char* file,
                                             int line) {
  bool matched = (0 == nr_strcmp(expected, actual));

  if (matched == expect_match) {
    tlib_did_pass();
  } else {
    char* cond
        = nr_formatf("0 %s nr_strcmp(%s, %s)",
                     expect_match ? "==" : "!=", NRSAFESTR(expected_literal),
                     NRSAFESTR(actual_literal));

    tlib_pass_if_true_f(what, 0, file, line, cond, "%s=\"%s\" %s=\"%s\"",
                        NRSAFESTR(expected_literal), NRSAFESTR(expected),
                        NRSAFESTR(actual_literal), NRSAFESTR(actual));

    nr_free(cond);
  }
}

/*
 * Generic pointer tests.
 */
#define tlib_pass_if_ptr_equal(M, EXPECTED, ACTUAL)                       \
  tlib_pass_if_equal((M), (const void*)(EXPECTED), (const void*)(ACTUAL), \
                     const void*, "%p")
#define tlib_fail_if_ptr_equal(M, EXPECTED, ACTUAL)                       \
  tlib_fail_if_equal((M), (const void*)(EXPECTED), (const void*)(ACTUAL), \
                     const void*, "%p")

/*
 * Null and non-null pointer tests.
 */
#define tlib_pass_if_null(M, ACTUAL) tlib_pass_if_ptr_equal((M), NULL, (ACTUAL))
#define tlib_fail_if_null(M, ACTUAL) tlib_fail_if_ptr_equal((M), NULL, (ACTUAL))
#define tlib_pass_if_not_null(M, ACTUAL) \
  tlib_fail_if_ptr_equal((M), NULL, (ACTUAL))
#define tlib_fail_if_not_null(M, ACTUAL) \
  tlib_pass_if_ptr_equal((M), NULL, (ACTUAL))

/*
 * Generic byte array tests.
 */
extern int tlib_pass_if_bytes_equal_f(const char* what,
                                      const void* expected,
                                      size_t expected_len,
                                      const void* actual,
                                      size_t actual_len,
                                      const char* file,
                                      int line);

extern int tlib_fail_if_bytes_equal_f(const char* what,
                                      void* expected,
                                      size_t expected_len,
                                      void* actual,
                                      size_t actual_len,
                                      const char* file,
                                      int line);

#define tlib_pass_if_bytes_equal(M, E, ELEN, A, ALEN) \
  tlib_pass_if_bytes_equal_f((M), (E), (ELEN), (A), (ALEN), __FILE__, __LINE__)

#define tlib_fail_if_bytes_equal(M, E, ELEN, A, ALEN) \
  tlib_pass_if_bytes_equal_f((M), (E), (ELEN), (A), (ALEN), __FILE__, __LINE__)

extern int nbsockpair(int vecs[2]);

#define test_pass_if_true(M, T, ...) \
  tlib_pass_if_true_f((M), (T), file, line, #T, __VA_ARGS__)

#define test_pass_if_true_file_line(M, T, FILE, LINE, ...) \
  tlib_pass_if_true_f((M), (T), (FILE), (LINE), #T, __VA_ARGS__)

/*
 * Purpose : Ignore sigpipe by installing a signal handler that does nothing.
 *           This allows testing of failed pipe writes.  The alternative is
 *           to block sigpipe, but this does not work with valgrind on OS X.
 */
extern void tlib_ignore_sigpipe(void);

/*
 * This is made C linkage so that stack dumps are otherwise identical
 * between C and C++ binaries.
 */
extern void test_main(void* p);

/*
 * Purpose : Make a non-blocking socket pair.
 */
extern int nbsockpair(int vecs[2]);

/*
 * Purpose : Test an object by comparing it to JSON.
 */
#define test_obj_as_json(T, O, J) \
  test_obj_as_json_fn((T), (O), (J), __FILE__, __LINE__)
extern void test_obj_as_json_fn(const char* testname,
                                const nrobj_t* obj,
                                const char* expected_json,
                                const char* file,
                                int line);

/*
 * Purpose : Get a thread local test specific structure.
 */
extern void* tlib_getspecific(void);

#endif /* TLIB_MAIN_HDR */
