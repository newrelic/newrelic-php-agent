/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "util_memory.h"
#include "util_regex.h"
#include "util_regex_private.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_regex_option_mapping_f(int pcre_option,
                                        int nr_option,
                                        const char* pcre_name,
                                        const char* nr_name) {
  char* message = NULL;
  unsigned long options = 0;
  nr_regex_t* regex = nr_regex_create(".", nr_option, 0);

  message = nr_formatf("%s <=> %s", pcre_name, nr_name);
  pcre_fullinfo(regex->code, regex->extra, PCRE_INFO_OPTIONS, (void*)&options);

  tlib_pass_if_true(message, options & pcre_option, "options=%lu expected=%d",
                    options, pcre_option);

  nr_free(message);
  nr_regex_destroy(&regex);
}

#define test_regex_option_mapping(pcre_option, nr_option)               \
  test_regex_option_mapping_f((pcre_option), (nr_option), #pcre_option, \
                              #nr_option)

static void test_regex_create(void) {
  nr_regex_t* regex = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL pattern", nr_regex_create(NULL, 0, 0));

  /*
   * Test : Option mapping.
   */
  test_regex_option_mapping(PCRE_ANCHORED, NR_REGEX_ANCHORED);
  test_regex_option_mapping(PCRE_CASELESS, NR_REGEX_CASELESS);
  test_regex_option_mapping(PCRE_DOLLAR_ENDONLY, NR_REGEX_DOLLAR_ENDONLY);
  test_regex_option_mapping(PCRE_DOTALL, NR_REGEX_DOTALL);
  test_regex_option_mapping(PCRE_MULTILINE, NR_REGEX_MULTILINE);

  /*
   * Test : Study.
   */
  regex = nr_regex_create(".", 0, 0);
  tlib_pass_if_not_null("no study", regex);
  tlib_pass_if_not_null("no study", regex->code);
  tlib_pass_if_null("no study", regex->extra);
  tlib_pass_if_int_equal("no study", 0, regex->capture_count);
  nr_regex_destroy(&regex);

  regex = nr_regex_create(".", 0, 1);
  tlib_pass_if_not_null("study", regex);
  tlib_pass_if_not_null("study", regex->code);
  tlib_pass_if_not_null("study", regex->extra);
  tlib_pass_if_int_equal("study", 0, regex->capture_count);
  nr_regex_destroy(&regex);
}

static void test_regex_destroy(void) {
  nr_regex_t* regex = nr_regex_create(".", 0, 1);
  nr_regex_t* regex_null = NULL;

  /*
   * Mostly, we just want to know we're not going to crash.
   */
  nr_regex_destroy(NULL);
  nr_regex_destroy(&regex_null);
  nr_regex_destroy(&regex);
  tlib_pass_if_null("destroy", regex);
}

static void test_regex_match(void) {
  nr_regex_t* regex = nr_regex_create("^[0-9]+$", 0, 1);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL regex",
                              nr_regex_match(NULL, NR_PSTR("foo")));
  tlib_pass_if_status_failure("NULL string", nr_regex_match(regex, NULL, 0));
  tlib_pass_if_status_failure("negative length",
                              nr_regex_match(regex, "foo", -1));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_failure("non matching",
                              nr_regex_match(regex, NR_PSTR("foo")));
  tlib_pass_if_status_success("matching",
                              nr_regex_match(regex, NR_PSTR("123")));

  nr_regex_destroy(&regex);
}

static void test_regex_match_capture(void) {
  nr_regex_t* regex = nr_regex_create("^[0-9]+$", 0, 1);
  nr_regex_substrings_t* ss = NULL;
  char* str = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL regex", nr_regex_match_capture(NULL, NR_PSTR("foo")));
  tlib_pass_if_null("NULL string", nr_regex_match_capture(regex, NULL, 0));
  tlib_pass_if_null("negative length",
                    nr_regex_match_capture(regex, "foo", -1));

  /*
   * Test : No matches.
   */
  tlib_pass_if_null("non matching",
                    nr_regex_match_capture(regex, NR_PSTR("foo")));

  /*
   * Test : Matched, but no subpatterns.
   */
  ss = nr_regex_match_capture(regex, NR_PSTR("123"));
  tlib_pass_if_not_null("no subpatterns", ss);
  tlib_pass_if_int_equal("no subpatterns", 0, nr_regex_substrings_count(ss));
  str = nr_regex_substrings_get(ss, 0);
  tlib_pass_if_str_equal("no subpatterns", "123", str);
  nr_free(str);
  nr_regex_substrings_destroy(&ss);

  /*
   * Test : Matched with subpatterns.
   */
  nr_regex_destroy(&regex);
  regex = nr_regex_create("^([a-z]+)-([0-9]+)$", 0, 0);
  ss = nr_regex_match_capture(regex, NR_PSTR("foo-123"));
  tlib_pass_if_not_null("subpatterns", ss);
  tlib_pass_if_int_equal("subpatterns", 2, nr_regex_substrings_count(ss));
  str = nr_regex_substrings_get(ss, 1);
  tlib_pass_if_str_equal("subpatterns", "foo", str);
  nr_free(str);
  nr_regex_substrings_destroy(&ss);

  nr_regex_destroy(&regex);
}

static void test_regex_substrings_create(void) {
  nr_regex_t* regex = nr_regex_create("^[0-9]+$", 0, 1);
  nr_regex_substrings_t* ss;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL code", nr_regex_substrings_create(NULL, 0));
  tlib_pass_if_null("negative count",
                    nr_regex_substrings_create(regex->code, -1));

  /*
   * Test : Normal operation.
   */
  ss = nr_regex_substrings_create(regex->code, 0);
  tlib_pass_if_not_null("0 count", ss);
  tlib_pass_if_ptr_equal("0 count", regex->code, ss->code);
  tlib_pass_if_null("0 count", ss->subject);
  tlib_pass_if_int_equal("0 count", 0, ss->capture_count);
  tlib_pass_if_int_equal("0 count", 3, ss->ovector_size);
  tlib_pass_if_not_null("0 count", ss->ovector);
  nr_regex_substrings_destroy(&ss);

  ss = nr_regex_substrings_create(regex->code, 1);
  tlib_pass_if_not_null("1 count", ss);
  tlib_pass_if_ptr_equal("1 count", regex->code, ss->code);
  tlib_pass_if_null("1 count", ss->subject);
  tlib_pass_if_int_equal("1 count", 1, ss->capture_count);
  tlib_pass_if_int_equal("1 count", 6, ss->ovector_size);
  tlib_pass_if_not_null("1 count", ss->ovector);
  nr_regex_substrings_destroy(&ss);

  nr_regex_destroy(&regex);
}

static void test_regex_substrings_destroy(void) {
  nr_regex_t* regex = nr_regex_create("^[0-9]+$", 0, 1);
  nr_regex_substrings_t* ss = nr_regex_substrings_create(regex->code, 1);
  nr_regex_substrings_t* ss_null = NULL;

  /*
   * Mostly, we just want to know we're not going to crash.
   */
  nr_regex_substrings_destroy(NULL);
  nr_regex_substrings_destroy(&ss_null);
  nr_regex_substrings_destroy(&ss);
  tlib_pass_if_null("destroy", ss);

  nr_regex_destroy(&regex);
}

static void test_regex_substrings_count(void) {
  nr_regex_t* regex = nr_regex_create("^[0-9]+$", 0, 1);
  nr_regex_substrings_t* ss = nr_regex_substrings_create(regex->code, 1);

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL ss", -1, nr_regex_substrings_count(NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("NULL ss", 1, nr_regex_substrings_count(ss));

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);
}

static void test_regex_substrings_get(void) {
  nr_regex_t* regex = nr_regex_create("^([a-z]+)-([0-9]+)$", 0, 0);
  nr_regex_substrings_t* ss = nr_regex_match_capture(regex, NR_PSTR("foo-123"));
  char* str;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL ss", nr_regex_substrings_get(NULL, 0));
  tlib_pass_if_null("negative index", nr_regex_substrings_get(ss, -1));
  tlib_pass_if_null("out of bounds index", nr_regex_substrings_get(ss, 3));

  /*
   * Test : Normal operation.
   */
  str = nr_regex_substrings_get(ss, 0);
  tlib_pass_if_str_equal("whole match", "foo-123", str);
  nr_free(str);

  str = nr_regex_substrings_get(ss, 2);
  tlib_pass_if_str_equal("subpattern match", "123", str);
  nr_free(str);

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);
}

static void test_regex_substrings_get_named(void) {
  nr_regex_t* regex = nr_regex_create(
      "^(?P<alpha>[a-z]+)-(?P<digits>[0-9]+)|(<?P<more_alpha>[a-z]+)$", 0, 0);
  nr_regex_substrings_t* ss = nr_regex_match_capture(regex, NR_PSTR("foo-123"));
  char* str;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL ss", nr_regex_substrings_get_named(NULL, "alpha"));
  tlib_pass_if_null("NULL name", nr_regex_substrings_get_named(ss, NULL));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null("empty name", nr_regex_substrings_get_named(ss, ""));
  tlib_pass_if_null("missing name", nr_regex_substrings_get_named(ss, "other"));

  str = nr_regex_substrings_get_named(ss, "alpha");
  tlib_pass_if_str_equal("actual name", "foo", str);
  nr_free(str);

  str = nr_regex_substrings_get_named(ss, "digits");
  tlib_pass_if_str_equal("actual name", "123", str);
  nr_free(str);

  tlib_pass_if_null("non-capturing group",
                    nr_regex_substrings_get_named(ss, "more_alpha"));

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);
}

static void test_regex_substrings_get_offsets(void) {
  int offsets[2] = {-1, -1};
  nr_regex_t* regex = nr_regex_create("^([a-z]+)-([0-9]+)$", 0, 0);
  nr_regex_substrings_t* ss = nr_regex_match_capture(regex, NR_PSTR("foo-123"));

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure(
      "NULL ss", nr_regex_substrings_get_offsets(NULL, 0, offsets));
  tlib_pass_if_status_failure("negative index",
                              nr_regex_substrings_get_offsets(ss, -1, offsets));
  tlib_pass_if_status_failure("out of bounds index",
                              nr_regex_substrings_get_offsets(ss, 3, offsets));
  tlib_pass_if_status_failure("NULL offsets",
                              nr_regex_substrings_get_offsets(ss, 0, NULL));

  tlib_pass_if_int_equal("unchanged offsets", -1, offsets[0]);
  tlib_pass_if_int_equal("unchanged offsets", -1, offsets[1]);

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_status_success("whole match",
                              nr_regex_substrings_get_offsets(ss, 0, offsets));
  tlib_pass_if_int_equal("whole match", 0, offsets[0]);
  tlib_pass_if_int_equal("whole match", 7, offsets[1]);

  tlib_pass_if_status_success("subpattern match",
                              nr_regex_substrings_get_offsets(ss, 2, offsets));
  tlib_pass_if_int_equal("subpattern match", 4, offsets[0]);
  tlib_pass_if_int_equal("subpattern match", 7, offsets[1]);

  nr_regex_substrings_destroy(&ss);
  nr_regex_destroy(&regex);
}

static void test_regex_quote(void) {
  char* quoted = NULL;
  size_t quoted_len = 1;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL string", nr_regex_quote(NULL, 0, &quoted_len));
  tlib_pass_if_size_t_equal("NULL string doesn't change quoted_len", 1,
                            quoted_len);

  /*
   * Test : Normal operation. Note that testing the specific quoting behaviour
   * is handled by the nr_regex_add_quoted_to_buffer() tests.
   */
  quoted = nr_regex_quote(NR_PSTR(""), &quoted_len);
  tlib_pass_if_str_equal("zero length string", "", quoted);
  tlib_pass_if_size_t_equal("zero length string", 0, quoted_len);
  nr_free(quoted);

  quoted = nr_regex_quote(NR_PSTR("foo.bar"), &quoted_len);
  tlib_pass_if_str_equal("with quoted_len", "foo\\.bar", quoted);
  tlib_pass_if_size_t_equal("with quoted_len", 8, quoted_len);
  nr_free(quoted);

  quoted = nr_regex_quote(NR_PSTR("foo.bar"), NULL);
  tlib_pass_if_str_equal("without quoted_len", "foo\\.bar", quoted);
  nr_free(quoted);
}

static void test_regex_escaping_f(const char* message,
                                  const char* expected,
                                  const char* input,
                                  size_t len) {
  nrbuf_t* buf;

  buf = nr_buffer_create(0, 0);

  nr_regex_add_quoted_to_buffer(buf, input, len);
  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal(message, expected, (const char*)nr_buffer_cptr(buf));

  nr_buffer_destroy(&buf);
}

#define test_regex_escaping(M, EXP, INPUT) \
  test_regex_escaping_f((M), (EXP), NR_PSTR(INPUT))

static void test_regex_add_quoted_to_buffer(void) {
  nrbuf_t* buf;

  buf = nr_buffer_create(0, 0);

  /*
   * Test : Bad parameters.
   */
  nr_regex_add_quoted_to_buffer(NULL, NR_PSTR("foo"));

  nr_regex_add_quoted_to_buffer(buf, NULL, 0);
  tlib_pass_if_int_equal("buffer length is 0", 0, nr_buffer_len(buf));

  /*
   * Test : Various escaping scenarios.
   */
  test_regex_escaping("empty string", "", "");
  test_regex_escaping("no escaping required", "foo", "foo");
  test_regex_escaping("NULL", "foo\\000bar", "foo\0bar");
  test_regex_escaping("dot", "\\.foo", ".foo");
  test_regex_escaping("backslash", "foo\\\\", "foo\\");
  test_regex_escaping("plus", "foo\\+bar", "foo+bar");
  test_regex_escaping("asterisk", "\\*foo", "*foo");
  test_regex_escaping("question mark", "foo\\?", "foo?");
  test_regex_escaping("square brackets", "foo\\[bar\\]", "foo[bar]");
  test_regex_escaping("caret", "foo\\^bar", "foo^bar");
  test_regex_escaping("dollar sign", "\\$foo", "$foo");
  test_regex_escaping("parentheses", "\\(foo\\)", "(foo)");
  test_regex_escaping("curly braces", "\\{foo\\}bar", "{foo}bar");
  test_regex_escaping("equals", "foo\\=bar", "foo=bar");
  test_regex_escaping("exclamation mark", "foo\\!", "foo!");
  test_regex_escaping("greater than", "foo\\>bar", "foo>bar");
  test_regex_escaping("less than", "foo\\<bar", "foo<bar");
  test_regex_escaping("pipe", "\\|foo", "|foo");
  test_regex_escaping("colon", "foo\\:", "foo:");
  test_regex_escaping("dash", "foo\\-bar", "foo-bar");

  nr_buffer_destroy(&buf);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_regex_create();
  test_regex_destroy();
  test_regex_match();
  test_regex_match_capture();
  test_regex_substrings_create();
  test_regex_substrings_destroy();
  test_regex_substrings_count();
  test_regex_substrings_get();
  test_regex_substrings_get_named();
  test_regex_substrings_get_offsets();
  test_regex_quote();
  test_regex_add_quoted_to_buffer();
}
