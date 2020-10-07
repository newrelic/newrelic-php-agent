/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>
#include <stdlib.h>

#include "util_json.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

static int test_nr_json_escape(char** dstp, const char* src) {
  int len = 0;

  if (0 == dstp) {
    return 0;
  }

  len = src ? nr_strlen(src) : 0;
  *dstp = (char*)nr_malloc(6 * len + 3);
  return nr_json_escape(*dstp, src);
}

static void test_json_worker(void) {
  char* dest = 0;
  int count;

  count = test_nr_json_escape(0, "");
  tlib_pass_if_true("NULL dest buffer", 0 == count, "NULL dest buffer");

  count = test_nr_json_escape(&dest, 0);
  tlib_pass_if_true("null json string", 0 == nr_strcmp("\"\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("null json string", count == 2, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "");
  tlib_pass_if_true("empty json string", 0 == nr_strcmp("\"\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("empty json string", count == 2, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "abcd");
  tlib_pass_if_true("abcd json string", 0 == nr_strcmp("\"abcd\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("abcd json string", count == 6, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\"");
  tlib_pass_if_true("double quote json string",
                    0 == nr_strcmp("\"\\\"\"", dest), "escaped string is %s",
                    dest);
  tlib_pass_if_true("double quote json string", count == 4,
                    "escaped string is %s", dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\n");
  tlib_pass_if_true("newline json string", 0 == nr_strcmp("\"\\n\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("newline json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\r");
  tlib_pass_if_true("return json string", 0 == nr_strcmp("\"\\r\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("return json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\f");
  tlib_pass_if_true("formfeed json string", 0 == nr_strcmp("\"\\f\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("formfeed json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\b");
  tlib_pass_if_true("backspace json string", 0 == nr_strcmp("\"\\b\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("backspace json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\t");
  tlib_pass_if_true("tab json string", 0 == nr_strcmp("\"\\t\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("tab json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\\");
  tlib_pass_if_true("backslash json string", 0 == nr_strcmp("\"\\\\\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("backslash json string", count == 4, "escaped string is %s",
                    dest);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "/");
  tlib_pass_if_true("forwardslash json string", 0 == nr_strcmp("\"\\/\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("forwardslash json string", count == 4,
                    "escaped string is %s", dest);
  nr_free(dest);

  /*
   * The GBP sign takes up 2 bytes.
   * See http://www.fileformat.info/info/unicode/char/a3/index.htm for Great
   * Britain Pound Sign hex value is 0xc2 0xa3 UTF-8 is \u00A3
   */
  count = test_nr_json_escape(&dest, "GBP sign £xxx");
  tlib_pass_if_true("character GBP json string",
                    0 == nr_strcmp("\"GBP sign \\u00a3xxx\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character GBP json string", count == 20, "count==%d",
                    count);
  nr_free(dest);

  /*
   * The same thing, but express the string in C-hex notation
   */
  count = test_nr_json_escape(&dest, "GBP sign \xc2\xa3xxx");
  tlib_pass_if_true("character GBP json string",
                    0 == nr_strcmp("\"GBP sign \\u00a3xxx\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character GBP json string", count == 20, "count==%d",
                    count);
  nr_free(dest);

  /*
   * The euro sign takes up 3 bytes.
   * See http://www.fileformat.info/info/unicode/char/20aC/index.htm for Euro
   * currency hex value is 0xe2 0x82 0xac UTF-8 is \u20ac
   */
  count = test_nr_json_escape(&dest, "Euro sign €xxx");
  tlib_pass_if_true("character Euro json string",
                    0 == nr_strcmp("\"Euro sign \\u20acxxx\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character Euro json string", count == 21, "count==%d",
                    count);
  nr_free(dest);

  /*
   * The same thing, but express the string in C-hex notation
   */
  count = test_nr_json_escape(&dest, "Euro sign \xe2\x82\xacxxx");
  tlib_pass_if_true("character Euro json string",
                    0 == nr_strcmp("\"Euro sign \\u20acxxx\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character Euro json string", count == 21, "count==%d",
                    count);
  nr_free(dest);

  /*
   * The Emoji character U+1F602 "Face with Tears of Joy" is in a code plane 01,
   * eg beyond the BMP. This is documented as an example encoding fault for
   * Json; See http://en.wikipedia.org/wiki/Json#Data_portability_issues See
   * http://en.wikipedia.org/wiki/Emoji
   *
   * This character needs to be encoded into java script using surrogate pairs
   *
   * See http://www.fileformat.info/info/unicode/char/1f602/index.htm
   * hex value is 0xF0 0x9F 0x98 0x82
   * UTF-8 is \u1f602
   */
  count = test_nr_json_escape(&dest, "Emoji Face with Tears of Joy 😂xxx");
  tlib_pass_if_true(
      "Single Emoji json string",
      0
          == nr_strcmp("\"Emoji Face with Tears of Joy \\ud83d\\ude02xxx\"",
                       dest),
      "escaped string is %s", dest);
  tlib_pass_if_true("Single Emoji json string", count == 46, "count==%d",
                    count);
  nr_free(dest);

  /*
   * Make sure we can have 2 concatenated utf-8 characters.
   */
  count = test_nr_json_escape(&dest,
                              "Doubled Emoji Face with Tears of Joy 😂😂xxx");
  tlib_pass_if_true("Doubled Emoji json string",
                    0
                        == nr_strcmp("\"Doubled Emoji Face with Tears of Joy "
                                     "\\ud83d\\ude02\\ud83d\\ude02xxx\"",
                                     dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("Doubled Emoji json string", count == 66, "count==%d",
                    count);
  nr_free(dest);

  /*
   * Make sure we can have 32 concatenated Emojis, with nothing else in the
   * buffer. This stresses the buffer management.
   */
  count = test_nr_json_escape(
      &dest,
      "😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂😂");
  tlib_pass_if_true("Long Emoji json string",
                    0
                        == nr_strcmp("\""
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\\ud83d\\ude02\\ud83d\\ude02"
                                     "\"",
                                     dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("32*Emoji", count == 770, "count==%d", count);
  nr_free(dest);

  /*
   * Give an illegal utf8 encoding of the Euro sign.
   * The last continuation byte is malformed: it is given here as 0xec rather
   * than 0xac; the leading 2 bits must be 0b10
   */
  count = test_nr_json_escape(&dest, "Mangled Euro sign \xe2\x82\xecxxx");
  tlib_pass_if_true(
      "invalid character Euro json string",
      0 == nr_strcmp("\"Mangled Euro sign \\u00e2\\u0082\\u00ecxxx\"", dest),
      "escaped string is %s", dest);
  tlib_pass_if_true("invalid character Euro json string", count == 41,
                    "count==%d", count);
  nr_free(dest);

  /*
   * Use a 26 bit (5 byte) encoding, which can't(?) be translated using
   * surrogate pairs. There does not seem to be an example of a 26-bit (5-byte)
   * encoding character, so here's a creative invention. The behavior of
   * the encoder is pretty much undefined.
   */
  count = test_nr_json_escape(&dest,
                              "26-bit encoding "
                              "\xfa"
                              "\xab"
                              "\xac"
                              "\xad"
                              "\xae"
                              "xxx");
  tlib_pass_if_true("26-bit encoding",
                    0
                        == nr_strcmp("\"26-bit encoding "
                                     "\\u00fa\\u00ab\\u00ac\\u00ad\\u00ae"
                                     "xxx\"",
                                     dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("26-bit encoding", count == 51, "count==%d", count);
  nr_free(dest);

  /*
   * Use a 31 bit (6 byte) encoding, which can't(?) be translated using
   * surrogate pairs.
   */
  count = test_nr_json_escape(&dest,
                              "31-bit encoding "
                              "\xfc"
                              "\xab"
                              "\xac"
                              "\xad"
                              "\xae"
                              "\xaf"
                              "xxx");
  tlib_pass_if_true(
      "31-bit encoding",
      0
          == nr_strcmp("\"31-bit encoding "
                       "\\u00fc\\u00ab\\u00ac\\u00ad\\u00ae\\u00af"
                       "xxx\"",
                       dest),
      "escaped string is %s", dest);
  tlib_pass_if_true("31-bit encoding", count == 57, "count==%d", count);
  nr_free(dest);

  /*
   * The translation of these escape sequences is likely to yield bogus UTF-8
   */
  count = test_nr_json_escape(&dest, "\x01");
  tlib_pass_if_true("character x01 json string",
                    0 == nr_strcmp("\"\\u0001\"", dest), "escaped string is %s",
                    dest);
  tlib_pass_if_true("character x01 json string", count == 8, "count==%d",
                    count);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\x01\x02");
  tlib_pass_if_true("character x01 json string",
                    0 == nr_strcmp("\"\\u0001\\u0002\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character x01 json string", count == 14, "count==%d",
                    count);
  nr_free(dest);

  count = test_nr_json_escape(&dest, "\x81\x82");
  tlib_pass_if_true("character x01 json string",
                    0 == nr_strcmp("\"\\u0081\\u0082\"", dest),
                    "escaped string is %s", dest);
  tlib_pass_if_true("character x01 json string", count == 14, "count==%d",
                    count);
  nr_free(dest);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 4, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_json_worker();
}
