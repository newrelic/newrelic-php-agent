/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "util_matcher.h"

#include "tlib_main.h"

static void test_match_multiple(void) {
  char* found;
  nr_matcher_t* matcher;

  matcher = nr_matcher_create();
  tlib_pass_if_bool_equal("add prefix", true,
                          nr_matcher_add_prefix(matcher, "/foo"));
  tlib_pass_if_bool_equal("add prefix", true,
                          nr_matcher_add_prefix(matcher, "/bar//"));

  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, ""));
  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, "foo"));
  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, "/bar"));

  found = nr_matcher_match(matcher, "/foo/baz/quux");
  tlib_pass_if_str_equal("needle match", "baz", found);
  nr_free(found);

  found = nr_matcher_match(matcher, "/foo/baz//quux");
  tlib_pass_if_str_equal("needle match", "baz", found);
  nr_free(found);

  found = nr_matcher_match(matcher, "/bar/xxx");
  tlib_pass_if_str_equal("needle match", "xxx", found);
  nr_free(found);

  nr_matcher_destroy(&matcher);
}

static void test_match_single(void) {
  char* found;
  nr_matcher_t* matcher = nr_matcher_create();

  nr_matcher_add_prefix(matcher, "/foo/bar");

  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, ""));
  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, "foo"));
  tlib_pass_if_null("needle not matched", nr_matcher_match(matcher, "/bar"));

  found = nr_matcher_match(matcher, "/foo/bar/quux");
  tlib_pass_if_str_equal("needle match", "quux", found);
  nr_free(found);

  found = nr_matcher_match(matcher, "/foo/bar//quux");
  tlib_pass_if_str_equal("needle match", "", found);
  nr_free(found);

  found = nr_matcher_match(matcher, "/foo/bar/quux/baz");
  tlib_pass_if_str_equal("needle match", "quux", found);
  nr_free(found);

  nr_matcher_destroy(&matcher);
}

static void test_match_ex(void) {
  char* found;
  int len;
  nr_matcher_t* matcher = nr_matcher_create();

  nr_matcher_add_prefix(matcher, "/foo/bar");
  found = nr_matcher_match_ex(matcher, NR_PSTR(""), &len);
  tlib_pass_if_null("needle not matched", found);
  tlib_pass_if_equal("needle match len", 0, len, int, "%d")
  nr_free(found);

  found = nr_matcher_match_ex(matcher, NR_PSTR("foo"), &len);
  tlib_pass_if_null("needle not matched", found);
  tlib_pass_if_equal("needle match len", 0, len, int, "%d")
  nr_free(found);

  found = nr_matcher_match_ex(matcher, NR_PSTR("/bar"), &len);
  tlib_pass_if_null("needle not matched", found);
  tlib_pass_if_equal("needle match len", 0, len, int, "%d")
  nr_free(found);

  found = nr_matcher_match_ex(matcher, NR_PSTR("/foo/bar/quux"), &len);
  tlib_pass_if_str_equal("needle match", "quux", found);
  tlib_pass_if_equal("needle match len", 4, len, int, "%d")
  nr_free(found);

  found = nr_matcher_match_ex(matcher, NR_PSTR("/foo/bar//quux"), &len);
  tlib_pass_if_str_equal("needle match", "", found);
  tlib_pass_if_equal("needle match len", 0, len, int, "%d")
  nr_free(found);

  found = nr_matcher_match_ex(matcher, NR_PSTR("/foo/bar/quux/baz"), &len);
  tlib_pass_if_str_equal("needle match", "quux", found);
  tlib_pass_if_equal("needle match len", 4, len, int, "%d")
  nr_free(found);

  nr_matcher_destroy(&matcher);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_match_multiple();
  test_match_single();
  test_match_ex();
}
