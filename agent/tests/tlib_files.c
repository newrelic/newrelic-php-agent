/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "nr_axiom.h"
#include "util_syscalls.h"
#include "tlib_main.h"

#include <stdio.h>
#include <unistd.h>

int tlib_pass_if_exists_f(const char* file, const char* f, int line) {
  if (0 == nr_access(file, F_OK)) {
    tlib_did_pass();
    return 0;
  }

  printf("FAIL [%s/%d]: existence check: %s\n", f, line, file);
  tlib_did_fail();
  return 1;
}

int tlib_pass_if_not_exists_f(const char* file, const char* f, int line) {
  if (0 == nr_access(file, F_OK)) {
    printf("FAIL [%s/%d]: absence check: %s\n", f, line, file);
    tlib_did_fail();
    return 1;
  }

  tlib_did_pass();
  return 0;
}
