/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include "nr_uuid.h"
#include "util_memory.h"

#include "tlib_main.h"
#include "util_sleep.h"

static void test_uuid_create(void) {
  char* uuid = NULL;
  char* tmp = NULL;

  uuid = nr_uuid_create(1234);
  tlib_pass_if_not_null("uuid create success", uuid);

  tmp = nr_strdup(uuid);
  nr_free(uuid);

  uuid = nr_uuid_create(4321);
  tlib_pass_if_true("new uuid != old uuid", 0 != nr_strcmp(tmp, uuid),
                    "old=%s, new=%s", tmp, uuid);

  nr_free(tmp);

  tmp = nr_strdup(uuid);

  nr_free(uuid);

  uuid = nr_uuid_create(0);
  tlib_pass_if_true("rand uuid != old uuid", 0 != nr_strcmp(tmp, uuid),
                    "old=%s, new=%s", tmp, uuid);

  nr_free(uuid);
  nr_free(tmp);
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_uuid_create();
}
