/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_configstrings.h"

#include "tlib_main.h"

static void test_nr_parse_time(void) {
  int i;
  nrtime_t t;
  struct {
    const char* testname;
    const char* input;
    nrtime_t expect;
  } testcases[] = {
      {"null string", NULL, 0},
      {"empty string", "", 0},
      {"bogus time string", "a", 0},
      {"h string", "h", 0},
      {"1msec", "1", 1000},
      {"2msec", "2", 2000},
      {"space 1msec", " 1", 1000},
      {"space 1msec", " 1   ", 1000},
      {"space 1msec", " 1\t", 1000},
      {"space 1msec", "\t\t1", 1000},
      {"1msec with ms suffix", "1ms", 1000},
      /* Numbers with franctional components aren't allowed, and return 0. */
      {"1msec with fraction", "1000.999", 0},
      {"0", "0", 0},
      {"0d", "0d", 0},
      /*
       * Now test the various suffixes, both cases.
       */
      {"1w", "1w", 1ULL * 7ULL * 86400 * 1000 * 1000},
      {"1d", "1d", 1ULL * 86400 * 1000 * 1000},
      {"1h", "1h", 1ULL * 3600 * 1000 * 1000},
      {"1m", "1m", 1ULL * 60 * 1000 * 1000},
      {"1s", "1s", 1ULL * 1000 * 1000},
      {"1ms", "1ms", 1ULL * 1000},
      {"1us", "1us", 1ULL * 1},
      {"1W", "1W", 1ULL * 7ULL * 86400 * 1000 * 1000},
      {"1D", "1D", 1ULL * 86400 * 1000 * 1000},
      {"1H", "1H", 1ULL * 3600 * 1000 * 1000},
      {"1M", "1M", 1ULL * 60 * 1000 * 1000},
      {"1S", "1S", 1ULL * 1000 * 1000},
      {"1MS", "1MS", 1ULL * 1000},
      {"1US", "1US", 1ULL * 1},
  };
  int num_testcases = sizeof(testcases) / sizeof(testcases[0]);

  t = nr_parse_time("-1000"); /* dangerous! */
  tlib_pass_if_true("1msec", 1000 * -1000LL == (int64_t)t, "t=" NR_TIME_FMT, t);

  for (i = 0; i < num_testcases; i++) {
    t = nr_parse_time(testcases[i].input);

    tlib_pass_if_true(testcases[i].testname, t == testcases[i].expect,
                      "t=" NR_TIME_FMT " expect=" NR_TIME_FMT " input=%s", t,
                      testcases[i].expect, NRSAFESTR(testcases[i].input));
  }
}

static void test_nr_bool_from_str(void) {
  int x;
  int i;
  struct {
    const char* input;
    int expect;
  } testcases[] = {
      {NULL, 0},
      {"", 0},
      {"0", 0},
      {"false", 0},
      {"f", 0},
      {"FalSe", 0},
      {"n", 0},
      {"no", 0},
      {"No", 0},
      {"Off", 0},
      {"DisABLE", 0},
      {"Disabled", 0},
      {"true", 1},
      {"1", 1},
      {"t", 1},
      {"TruE", 1},
      {"y", 1},
      {"yes", 1},
      {"Yes", 1},
      {"On", 1},
      {"Enabled", 1},
      {"ENABLE", 1},
      /* Error cases. */
      {"7", -1},
      {" On", -1}, /* alas, we don't ignore spaces */
      {"On ", -1}, /* alas, we don't ignore spaces */
      {"Off7", -1},
      {"On7", -1},
  };
  int num_testcases = sizeof(testcases) / sizeof(testcases[0]);

  for (i = 0; i < num_testcases; i++) {
    x = nr_bool_from_str(testcases[i].input);

    tlib_pass_if_true(testcases[i].input ? testcases[i].input : "NULL",
                      x == testcases[i].expect, "x=%d expect=%d input=%s", x,
                      testcases[i].expect, NRSAFESTR(testcases[i].input));
  }
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_nr_parse_time();
  test_nr_bool_from_str();
}
