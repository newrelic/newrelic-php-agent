/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <time.h>

#include "nr_uuid.h"
#include "util_memory.h"

static const char hex_values[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
char* nr_uuid_create(int seed) {
  char* uuid = nr_zalloc(NR_UUID_SIZE + 1);
  if (0 < seed) {
    srand(seed);
  } else {
    srand(time(NULL));
  }

  for (int i = 0; i < NR_UUID_SIZE; i++) {
    int r = rand() % NR_UUID_RANGE;

    uuid[i] = hex_values[r];
  }

  return uuid;
}
