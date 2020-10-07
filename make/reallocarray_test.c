/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

int main(void) {
  free(reallocarray(NULL, 8, 8));
  return 0;
}
