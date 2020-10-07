/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_guid.h"

#include "util_memory.h"

static const char* hex_digits = "0123456789abcdef";

char* nr_guid_create(nr_random_t* rnd) {
  char* guid = nr_zalloc(NR_GUID_SIZE + 1);
  size_t i;

  for (i = 0; i < NR_GUID_SIZE; i++) {
    unsigned long r = nr_random_range(rnd, 0xf);

    guid[i] = hex_digits[r];
  }

  return guid;
}
