/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_sleep.h"
#include "util_time.h"

#include "tlib_main.h"

/*
 * Some floating point decimal to binary routines differ in 1 ULP
 * when converting microsecond precise time to microseconds.
 */
static int fuzz_time_match(uint64_t expected, uint64_t actual) {
  return (expected == actual) || (expected - 1ULL == actual)
         || (expected + 1ULL == actual);
}

static void test_duration(void) {
  tlib_pass_if_uint64_t_equal("start > stop", 0, nr_time_duration(1, 0));
  tlib_pass_if_uint64_t_equal("start == stop", 0, nr_time_duration(1, 1));
  tlib_pass_if_uint64_t_equal("start < stop", 1, nr_time_duration(0, 1));
}

static void test_parse_unix_time(void) {
  nrtime_t t1;

  t1 = nr_parse_unix_time(0);
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("nope");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("t");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("0");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("0.0");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("9999999999999999999999999999999999999999999999999");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("-1368811467146000");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("3000000000");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("3000000000000");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("900000000");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("900000000000");
  tlib_pass_if_true("parse bad unix time", 0 == t1, "t1=" NR_TIME_FMT, t1);

  /*
   * Microseconds
   */
  t1 = nr_parse_unix_time("1368811467146000");
  tlib_pass_if_true("parse unix time", 1368811467146000ULL == t1,
                    "t1=" NR_TIME_FMT, t1);

  /*
   * Milliseconds
   */
  t1 = nr_parse_unix_time("1368811467146.000");
  tlib_pass_if_true("parse unix time", 1368811467146000ULL == t1,
                    "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("1368811467146");
  tlib_pass_if_true("parse unix time", 1368811467146000ULL == t1,
                    "t1=" NR_TIME_FMT, t1);

  /*
   * Seconds
   */
  t1 = nr_parse_unix_time("1368811467.146000");
  tlib_pass_if_true("parse unix time", fuzz_time_match(1368811467146000ULL, t1),
                    "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("1368811467.146");
  tlib_pass_if_true("parse unix time", fuzz_time_match(1368811467146000ULL, t1),
                    "t1=" NR_TIME_FMT, t1);
  t1 = nr_parse_unix_time("1368811467");
  tlib_pass_if_true("parse unix time", fuzz_time_match(1368811467000000ULL, t1),
                    "t1=" NR_TIME_FMT, t1);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  /*
   * It is tempting to test nr_msleep and nr_usleep, but those tests
   * consistently fail when run on virtual machines (VMs). The VMs cause
   * time dilation due to multiple layers of process stoppage, the weird
   * notion of real time on a VM, as well as the guest O/S (especially
   * on solaris or BSD) not being configured to support a high enough
   * clock interrupt rate.
   */

  test_parse_unix_time();

  test_duration();
}
