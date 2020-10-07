/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_base64.h"
#include "util_memory.h"
#include "util_obfuscate.h"
#include "util_strings.h"

#include "tlib_main.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  char* rv;
  char* rp;

  /*
   * Test : Bad Parameters to nr_obfuscate
   */
  rv = nr_obfuscate(0, 0, 0);
  tlib_pass_if_true("zero params", 0 == rv, "rv=%p", rv);

  rv = nr_obfuscate(0, "BLAHHHH", 0);
  tlib_pass_if_true("null string", 0 == rv, "rv=%p", rv);

  rv = nr_obfuscate("", "BLAHHHH", 0);
  tlib_pass_if_true("empty string", 0 == rv, "rv=%p", rv);

  rv = nr_obfuscate("testString", 0, 0);
  tlib_pass_if_true("null key", 0 == rv, "rv=%p", rv);

  rv = nr_obfuscate("testString", "", 0);
  tlib_pass_if_true("empty key", 0 == rv, "rv=%p", rv);

  rv = nr_obfuscate("testString", "BLAHHHH", -1);
  tlib_pass_if_true("negative keylen", 0 == rv, "rv=%p", rv);

  /*
   * Test : Bad Parameters to nr_deobfuscate
   */
  rv = nr_deobfuscate(0, 0, 0);
  tlib_pass_if_true("zero params", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate(0, "BLAHHHH", 0);
  tlib_pass_if_true("null string", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate("", "BLAHHHH", 0);
  tlib_pass_if_true("empty string", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate("NikyPBs8OisiJg==", 0, 0);
  tlib_pass_if_true("null key", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate("NikyPBs8OisiJg==", "", 0);
  tlib_pass_if_true("empty key", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate("NikyPBs8OisiJg==", "BLAHHHH", -1);
  tlib_pass_if_true("negative keylen", 0 == rv, "rv=%p", rv);

  rv = nr_deobfuscate("==", "BLAHHHH", 0);
  tlib_pass_if_true("decode fails", 0 == rv, "rv=%p", rv);

  /*
   * Test : Successful Usage
   */
  rv = nr_obfuscate("testString", "BLAHHHH", 0);
  tlib_pass_if_true("obfuscate success", 0 == nr_strcmp(rv, "NikyPBs8OisiJg=="),
                    "rv=%s", NRSAFESTR(rv));
  rp = nr_deobfuscate(rv, "BLAHHHH", 0);
  tlib_pass_if_true("deobfuscate success", 0 == nr_strcmp(rp, "testString"),
                    "rp=%s", NRSAFESTR(rp));
  nr_free(rv);
  nr_free(rp);

  /*
   * Test : Successful Usage keylen Provided
   */
  rv = nr_obfuscate("testString", "BLAHHHH", nr_strlen("BLAHHHH"));
  tlib_pass_if_true("obfuscate success", 0 == nr_strcmp(rv, "NikyPBs8OisiJg=="),
                    "rv=%s", NRSAFESTR(rv));
  rp = nr_deobfuscate(rv, "BLAHHHH", nr_strlen("BLAHHHH"));
  tlib_pass_if_true("deobfuscate success", 0 == nr_strcmp(rp, "testString"),
                    "rp=%s", NRSAFESTR(rp));
  nr_free(rv);
  nr_free(rp);

  /*
   * Test : Successful Usage Short keylen
   */
  rv = nr_obfuscate("testString", "BLAHHHH", 3);
  tlib_pass_if_true("obfuscate success", 0 == nr_strcmp(rv, "NikyNh81MCUvJQ=="),
                    "rv=%s", NRSAFESTR(rv));
  rp = nr_deobfuscate(rv, "BLAHHHH", 3);
  tlib_pass_if_true("deobfuscate success", 0 == nr_strcmp(rp, "testString"),
                    "rp=%s", NRSAFESTR(rp));
  nr_free(rv);
  nr_free(rp);
}
