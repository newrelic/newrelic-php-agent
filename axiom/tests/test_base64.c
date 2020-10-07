/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_base64.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

#define TC(R, E) {R, (sizeof(R) - 1), E, (sizeof(E) - 1)},

struct testcase {
  const char* raw;
  int raw_len;
  const char* enc;
  int enc_len;
};
struct testcase testcases[] = {
    TC("\x00", "AA==") TC("\x01", "AQ==") TC(
        "\xb2\x2a\x81\x8f\xbd\x6\xfd\xa5\xe9\xf2\xee\x57\xe6\xca\x9e\xa9\xcf"
        "\x9e\x4e",
        "siqBj70G/aXp8u5X5sqeqc+eTg==") TC("\x4a\xe3\xf4\x85\x76\xf\xb1\xb4\x83"
                                           "\x38\x75\xc6\x86\xe3\xd8\x6e\x71"
                                           "\x37\x5\x9b\x2f\xe8",
                                           "SuP0hXYPsbSDOHXGhuPYbnE3BZsv6A==")
        TC("\x6e\xe2\xb9\x36\xe1\xf\xd0", "buK5NuEP0A==") TC("\x68\x65", "aGU=")
            TC("\xeb\x89\xac\x83\x3c\xf0\xc1\xb1",
               "64msgzzwwbE=") TC("\xf9\xc\x85\x96\x4b\x94\xc3", "+QyFlkuUww==")
                TC("\x67\xb3\xef\x9d\xbf", "Z7Pvnb8=") TC(
                    "\xe6\x59\x6d\x4e\x76\xb7\x20\x9e\xf1\x55\xb7\xc2\x97\x38"
                    "\xce\x24\x3b",
                    "5lltTna3IJ7xVbfClzjOJDs=") TC("\x2a\x95\xc9", "KpXJ")
                    TC("\xb3\xba\x4b\x3b\x26\x8d\x51\xd4\x1d\xba\x2\xb3\xae\x39"
                       "\xce\xd6\x63",
                       "s7pLOyaNUdQdugKzrjnO1mM=")
                        TC("\xa\x48\x7e\x4c\x6f\xd7\x9\x29\xfb\x7b\x81\xbf\xa2"
                           "\xd3\x84\xaf\xad\xb2",
                           "Ckh+TG/XCSn7e4G/otOEr62y")
                            TC("\xf4\xf\xc1\xa6\xf4\x59\x83", "9A/BpvRZgw==")
                                TC("\x13\xe2\xe\x77\x6b\xf4", "E+IOd2v0")
                                    TC("\x64", "ZA=="){0, 0, 0, 0},
};

static void valid_character_testcase(char c, int expected_rv) {
  int actual_rv;

  actual_rv = nr_b64_is_valid_character(c);
  tlib_pass_if_true("valid character", expected_rv == actual_rv,
                    "c=%c expected_rv=%d actual_rv=%d", c, expected_rv,
                    actual_rv);
}

static void test_is_valid_character(void) {
  int i;
  const char* valid_chars
      = "0123456789"
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+/=";

  for (i = 0; valid_chars[i]; i++) {
    valid_character_testcase(valid_chars[i], 1);
  }

  valid_character_testcase('_', 0);
  valid_character_testcase('-', 0);
  valid_character_testcase('\n', 0);
  valid_character_testcase('\'', 0);
  valid_character_testcase('"', 0);
  valid_character_testcase('^', 0);
  valid_character_testcase('@', 0);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  int i;
  int len;
  char* s;

  /*
   * Test 1: invalid parameters.
   */
  s = nr_b64_encode(0, 5, &len);
  tlib_pass_if_true("encode with NULL data", 0 == s, "s=%p", s);
  s = nr_b64_encode("abcde", -1, &len);
  tlib_pass_if_true("encode with negative len", 0 == s, "s=%p", s);
  s = nr_b64_encode("abcde", 0, &len);
  tlib_pass_if_true("encode with zero len", 0 == s, "s=%p", s);
  s = nr_b64_decode(0, &len);
  tlib_pass_if_true("decode with NULL src", 0 == s, "s=%p", s);
  s = nr_b64_decode("", &len);
  tlib_pass_if_true("encode with empty src", 0 == s, "s=%p", s);

  for (i = 0; testcases[i].raw; i++) {
    /*
     * Test 2: Encode.
     */
    s = nr_b64_encode(testcases[i].raw, testcases[i].raw_len, &len);
    tlib_pass_if_true("encode returns correct string",
                      0 == nr_strcmp(s, testcases[i].enc),
                      "i=%d s=%s testcases[i].enc=%s", i, s, testcases[i].enc);
    tlib_pass_if_true(
        "encode returns correct length", len == testcases[i].enc_len,
        "i=%d len=%d testcases[i].enc_len=%d", i, len, testcases[i].enc_len);
    nr_free(s);
    s = nr_b64_encode(testcases[i].raw, testcases[i].raw_len, 0);
    tlib_pass_if_true("encode without retlen",
                      0 == nr_strcmp(s, testcases[i].enc),
                      "i=%d s=%s testcases[i].enc=%s", i, s, testcases[i].enc);
    nr_free(s);

    /*
     * Test 3: Decode.
     */
    s = nr_b64_decode(testcases[i].enc, &len);
    tlib_pass_if_true("decode returns correct string",
                      0 == nr_strcmp(s, testcases[i].raw),
                      "i=%d s=%p testcases[i].raw=%p", i, s, testcases[i].raw);
    tlib_pass_if_true(
        "decode returns correct length", len == testcases[i].raw_len,
        "i=%d len=%d testcases[i].raw_len=%d", i, len, testcases[i].raw_len);
    nr_free(s);
    s = nr_b64_decode(testcases[i].enc, 0);
    tlib_pass_if_true("decode without retlen",
                      0 == nr_strcmp(s, testcases[i].raw),
                      "i=%d s=%s testcases[i].raw=%s", i, s, testcases[i].raw);
    nr_free(s);

    s = nr_b64_decode("!!!!", 0);
    tlib_pass_if_true("decode improper string", 0 == s, "s=%p", s);
    s = nr_b64_decode("@", 0);
    tlib_pass_if_true("decode improper string", 0 == s, "s=%p", s);
  }

  test_is_valid_character();
}
