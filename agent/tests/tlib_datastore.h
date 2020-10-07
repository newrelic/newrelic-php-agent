/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TLIB_DATASTORE_HDR
#define TLIB_DATASTORE_HDR

#include "nr_datastore_instance.h"

extern void assert_datastore_instance_equals_f(
    const char* message,
    const nr_datastore_instance_t* expected,
    const nr_datastore_instance_t* actual,
    const char* file,
    int line);

#define assert_datastore_instance_equals(MSG, EXPECTED, ACTUAL)             \
  assert_datastore_instance_equals_f((MSG), (EXPECTED), (ACTUAL), __FILE__, \
                                     __LINE__)

#define assert_datastore_instance_equals_destroy(MSG, EXPECTED, ACTUAL)       \
  do {                                                                        \
    nr_datastore_instance_t* __actual = (ACTUAL);                             \
                                                                              \
    assert_datastore_instance_equals_f((MSG), (EXPECTED), __actual, __FILE__, \
                                       __LINE__);                             \
    nr_datastore_instance_destroy(&__actual);                                 \
  } while (0)

#endif /* TLIB_DATASTORE_HDR */
