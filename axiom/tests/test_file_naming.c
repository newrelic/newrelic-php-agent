/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "nr_file_naming.h"
#include "nr_file_naming_private.h"
#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

static void file_namer_test_null_match(const char* name,
                                       nr_file_naming_t* namers,
                                       const char* filename) {
  char* match;

  match = nr_file_namer_match(namers, filename);
  tlib_pass_if_null(name, match);
}

static void test_file_namer(void) {
  nr_file_naming_t* namers = NULL;
  char* match = NULL;

  namers = nr_file_namer_append(NULL, "");
  tlib_pass_if_null("Empty filename fails to create a list", namers);

  namers = nr_file_namer_append(NULL, NULL);
  tlib_pass_if_null("Null filename fails to create a list", namers);
  file_namer_test_null_match("Empty name list doesn't match", NULL,
                             "docs/a.php");
  file_namer_test_null_match("Empty name list doesn't match empty string", NULL,
                             "");

  namers = nr_file_namer_append(NULL, "foo");
  tlib_pass_if_not_null("Appending to a NULL list results in a list", namers);
  tlib_pass_if_null("nr_file_namer_append initializes next to NULL",
                    namers->next);
  tlib_pass_if_str_equal("user_pattern is set", "foo", namers->user_pattern);

  namers = nr_file_namer_append(namers, NULL);
  tlib_pass_if_ptr_equal("Null filename fails to add to list", NULL,
                         namers->next);
  file_namer_test_null_match("Null name doesn't match", namers, NULL);
  file_namer_test_null_match("Empty filename doesn't match", namers, NULL);

  match = nr_file_namer_match(NULL, NULL);
  tlib_pass_if_ptr_equal("Name list NULL and NULL doesn't crash or match", NULL,
                         match);

  namers = nr_file_namer_append(namers, "foo");
  namers = nr_file_namer_append(namers, "bar");

  match = nr_file_namer_match(namers, "foobar");
  tlib_pass_if_str_equal("Last match appended matches first", "bar", match);
  nr_free(match);

  tlib_pass_if_not_null("Linked list actually works", namers->next);
  nr_file_namer_destroy(&namers);
  tlib_pass_if_ptr_equal("Destructor sets the pointer passed to NULL", NULL,
                         namers);
}

static void regexp_tester(const char* test_name,
                          const char* user_expression,
                          const char* match_this,
                          const char* expected) {
  nr_file_naming_t* namers = nr_file_namer_append(NULL, user_expression);
  char* match;

  match = nr_file_namer_match(namers, match_this);
  tlib_pass_if_str_equal(test_name, expected, match);

  nr_file_namer_destroy(&namers);
  nr_free(match);
}

static void test_file_namer_regexes(void) {
  regexp_tester("Basic usage", "alpha", "alpha.php", "alpha");
  regexp_tester("Basic regexes the first", "[a-zA-Z]_?[0-9]{2,3}", "brain21",
                "n21");
  regexp_tester("Basic regexes the second", "[a-zA-Z]+_?[0-9]{2,3}",
                "ab_3335/test.php", "b_333");
  regexp_tester("Path match", "test/", "tests/test/.", "test/.");
  regexp_tester("Path match", "test/", "tests/test/.something.php", "test/.");
  regexp_tester("Path match", "test/", "tests/test/..", "test/..");
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_file_namer();
  test_file_namer_regexes();
}
