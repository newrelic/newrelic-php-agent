/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_datastore.h"
#include "tlib_php.h"

#include <assert.h>

#include "nr_datastore_instance.h"
#include "util_memory.h"
#include "util_strings.h"

static void assert_datastore_instance_field_equals_f(const char* message,
                                                     const char* field,
                                                     const char* expected,
                                                     const char* actual,
                                                     const char* file,
                                                     int line) {
  char* test_message = nr_formatf("%s: %s", message, field);

  if (expected) {
    tlib_pass_if_true_f(test_message, 0 == nr_strcmp(expected, actual), file,
                        line, "field doesn't match",
                        "field=%s expected=%s actual=%s", field, expected,
                        NRSAFESTR(actual));
  } else {
    tlib_pass_if_true_f(test_message, NULL == actual, file, line,
                        "field is not NULL", "field=%s expected=NULL actual=%s",
                        field, actual);
  }

  nr_free(test_message);
}

#define assert_datastore_instance_field_equals(MSG, FIELD, EXPECTED, ACTUAL, \
                                               F, L)                         \
  do {                                                                       \
    const char* __field = #FIELD;                                            \
    const char* __expected = (EXPECTED)->FIELD;                              \
    const char* __actual = (ACTUAL)->FIELD;                                  \
                                                                             \
    assert_datastore_instance_field_equals_f((MSG), __field, __expected,     \
                                             __actual, F, L);                \
  } while (0);

void assert_datastore_instance_equals_f(const char* message,
                                        const nr_datastore_instance_t* expected,
                                        const nr_datastore_instance_t* actual,
                                        const char* file,
                                        int line) {
  assert_datastore_instance_field_equals(message, database_name, expected,
                                         actual, file, line);

  assert_datastore_instance_field_equals(message, host, expected, actual, file,
                                         line);

  assert_datastore_instance_field_equals(message, port_path_or_id, expected,
                                         actual, file, line);
}
