/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>

#include "util_memory.h"
#include "util_serialize.h"
#include "util_strings.h"

#include "tlib_main.h"

static void test_get_class_name(void) {
  char* name;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL data", nr_serialize_get_class_name(NULL, 1));
  tlib_pass_if_null("0 data_len", nr_serialize_get_class_name("", 0));

  /*
   * Test : Non-object data.
   */
  tlib_pass_if_null("string",
                    nr_serialize_get_class_name(NR_PSTR("s:3:\"foo\";")));
  tlib_pass_if_null("boolean", nr_serialize_get_class_name(NR_PSTR("b:1;")));
  tlib_pass_if_null("integer", nr_serialize_get_class_name(NR_PSTR("i:42;")));
  tlib_pass_if_null("double", nr_serialize_get_class_name(NR_PSTR("d:42;")));
  tlib_pass_if_null("array", nr_serialize_get_class_name(NR_PSTR("a:0:{}")));
  tlib_pass_if_null("null", nr_serialize_get_class_name(NR_PSTR("N;")));

  /*
   * Test : Malformed object data.
   */
  tlib_pass_if_null("missing length",
                    nr_serialize_get_class_name(NR_PSTR("O::\"Foo\\Bar\":")));
  tlib_pass_if_null("missing colon",
                    nr_serialize_get_class_name(NR_PSTR("O:7:\"Foo\\Bar\"")));

  /*
   * Test : Well formed object data.
   */
  name = nr_serialize_get_class_name(NR_PSTR("O:7:\"Foo\\Bar\":0:{}"));
  tlib_pass_if_str_equal("valid object", "Foo\\Bar", name);

  nr_free(name);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_get_class_name();
}
