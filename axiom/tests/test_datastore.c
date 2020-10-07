/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_datastore.h"

#include "util_strings.h"

#include "tlib_main.h"
#include "test_segment_helpers.h"

static void test_as_string(void) {
  /*
   * This isn't intended to be an exhaustive set of tests: we're more interested
   * in the behaviour in the error cases here.
   */
  tlib_pass_if_str_equal("known datastore", "MySQL",
                         nr_datastore_as_string(NR_DATASTORE_MYSQL));
  tlib_pass_if_null("other datastore",
                    nr_datastore_as_string(NR_DATASTORE_OTHER));
  tlib_pass_if_null("unknown datastore",
                    nr_datastore_as_string(NR_DATASTORE_MUST_BE_LAST + 1));
}

static void test_from_string(void) {
  /*
   * This isn't intended to be an exhaustive set of tests: we're more interested
   * in the behaviour in the error cases here.
   */
  tlib_pass_if_int_equal("known datastore; normal case", NR_DATASTORE_MONGODB,
                         nr_datastore_from_string("MongoDB"));
  tlib_pass_if_int_equal("known datastore; abnormal case", NR_DATASTORE_MONGODB,
                         nr_datastore_from_string("mONGOdb"));
  tlib_pass_if_int_equal("other datastore", NR_DATASTORE_OTHER,
                         nr_datastore_from_string("foobar"));
  tlib_pass_if_int_equal("NULL datastore", NR_DATASTORE_OTHER,
                         nr_datastore_from_string(NULL));
}

static void test_is_sql(void) {
  /*
   * This isn't intended to be an exhaustive set of tests: we're more interested
   * in the behaviour in the error cases here.
   */
  tlib_fail_if_int_equal("SQL datastore", 0,
                         nr_datastore_is_sql(NR_DATASTORE_MYSQL));
  tlib_pass_if_int_equal("non-SQL datastore", 0,
                         nr_datastore_is_sql(NR_DATASTORE_MEMCACHE));
  tlib_pass_if_int_equal("other datastore", 0,
                         nr_datastore_is_sql(NR_DATASTORE_OTHER));
  tlib_pass_if_int_equal("unknown datastore", 0,
                         nr_datastore_is_sql(NR_DATASTORE_MUST_BE_LAST + 1));
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_as_string();
  test_from_string();
  test_is_sql();
}
