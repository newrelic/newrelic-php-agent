/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "util_memory.h"
#include "util_threads.h"

#include "tlib_main.h"

/*
 * The testing functions in this file follow
 * unix process status conventions: 0 on success, and 1 on failure,
 */

static nrthread_mutex_t tlib_passcount_mutex = NRTHREAD_MUTEX_INITIALIZER;

int tlib_did_pass(void) {
  nrt_mutex_lock(&tlib_passcount_mutex);
  { tlib_passcount += 1; }
  nrt_mutex_unlock(&tlib_passcount_mutex);
  return 0; /* Pass */
}

int tlib_did_fail(void) {
  nrt_mutex_lock(&tlib_passcount_mutex);
  { tlib_unexpected_failcount += 1; }
  nrt_mutex_unlock(&tlib_passcount_mutex);
  return 1; /* Fail */
}

int tlib_pass_if_true_f(const char* what,
                        int val,
                        const char* file,
                        int line,
                        const char* cond,
                        const char* fmt,
                        ...) {
  va_list ap;

  if (0 != val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: TRUE check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);

  return tlib_did_fail();
}

int tlib_pass_if_false_f(const char* what,
                         int val,
                         const char* file,
                         int line,
                         const char* cond,
                         const char* fmt,
                         ...) {
  va_list ap;

  if (0 == val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: FALSE check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);

  return tlib_did_fail();
}

int tlib_fail_if_true_f(const char* what,
                        int val,
                        const char* file,
                        int line,
                        const char* cond,
                        const char* fmt,
                        ...) {
  va_list ap;

  if (0 == val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: !TRUE check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);

  return tlib_did_fail();
}

int tlib_fail_if_false_f(const char* what,
                         int val,
                         const char* file,
                         int line,
                         const char* cond,
                         const char* fmt,
                         ...) {
  va_list ap;

  if (0 != val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: !FALSE check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
  fflush(stdout);

  return tlib_did_fail();
}

int tlib_pass_if_status_success_f(const char* what,
                                  nr_status_t val,
                                  const char* file,
                                  int line,
                                  const char* cond) {
  if (NR_SUCCESS == val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: NR_SUCCESS check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> Result: %d\n", (int)val);
  fflush(stdout);

  return tlib_did_fail();
}

int tlib_fail_if_status_success_f(const char* what,
                                  nr_status_t val,
                                  const char* file,
                                  int line,
                                  const char* cond) {
  if (NR_SUCCESS != val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: !NR_SUCCESS check: %s\n", file, line, what);
  printf(">>> Condition: %s\n", cond);
  printf(">>> Result: %d\n", (int)val);
  fflush(stdout);

  return tlib_did_fail();
}

/*
 * Mimic the output of xxd(1) to pretty print an array of bytes.
 */
static void hexdump(const char* p, size_t len) {
  size_t i;
  size_t j;
  const size_t bytes_per_line = 16;

  i = 0;
  do {
    printf(">>>   %07zx:", i);

    /* First print bytes as hex digits. */
    for (j = 0; j < bytes_per_line; j++) {
      /* Put a blank every 2 bytes. */
      if (j % 2 == 0) {
        printf(" ");
      }

      if (i + j < len) {
        printf("%02x", ((const uint8_t*)p)[i + j]);
      } else {
        printf("  ");
      }
    }

    printf(" ");

    /* Then print bytes as characters, if printable. */
    for (j = 0; j < bytes_per_line; j++) {
      if (i + j < len) {
        if (isprint((int)((const uint8_t*)p)[i + j])) {
          printf("%c", (char)((const uint8_t*)p)[i + j]);
        } else {
          printf(".");
        }
      } else {
        printf(" ");
      }
    }

    printf("\n");
    i += bytes_per_line;
  } while (i < len);
}

int tlib_pass_if_bytes_equal_f(const char* what,
                               const void* expected,
                               size_t expected_len,
                               const void* actual,
                               size_t actual_len,
                               const char* file,
                               int line) {
  int val;

  if (expected_len != actual_len) {
    printf("FAIL [%s:%d]: TRUE check: %s\n", file, line, what);
    printf(">>> Condition: expected_len == actual_len\n");
    printf(">>> Result: %zu != %zu\n", expected_len, actual_len);
    printf(">>> Expected:\n");
    hexdump((const char*)expected, expected_len);
    printf(">>> Actual:\n");
    hexdump((const char*)actual, actual_len);
    fflush(stdout);
    return tlib_did_fail();
  }

  val = nr_memcmp(expected, actual, expected_len);
  if (0 == val) {
    return tlib_did_pass();
  }

  printf("FAIL [%s:%d]: TRUE check: %s\n", file, line, what);
  printf(">>> Condition: 0 == nr_memcmp(expected, actual, expected_len)\n");
  printf(">>> Result: %d\n", val);
  printf(">>> Expected:\n");
  hexdump((const char*)expected, expected_len);
  printf(">>> Actual:\n");
  hexdump((const char*)actual, actual_len);
  fflush(stdout);
  return tlib_did_fail();
}
