/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "util_memory.h"
#include "util_strings.h"

#include "tlib_main.h"

nr_status_t tlib_pass_if_exec_f(const char* what,
                                const char* cmd,
                                int notdiff,
                                const char* file,
                                int line) {
  FILE* oput = popen(cmd, "r");
  char* buf = 0;
  char tmp[4096];
  int blen = 0;
  int slen;

  while (0 != (slen = (int)fread(tmp, 1, sizeof(tmp) - 1, oput))) {
    tmp[slen] = 0;
    buf = (char*)nr_realloc(buf, blen + slen + 2);
    nr_strcpy(buf + blen, tmp);
    blen += slen;
  }

  slen = pclose(oput);

  if (0 != slen) {
    printf("FAIL [%s:%d]: exec: %s\n", file, line, what);
    printf(">>> Command: %s\n", cmd);
    if (0 == notdiff) {
      printf(
          ">>> Output from diff is below. Lines beginning with a + are lines "
          "that\n");
      printf(
          ">>> appear in the generated file but not in the reference file, "
          "and\n");
      printf(
          ">>> lines that begin with a - are lines that appear in the "
          "reference\n");
      printf(">>> file but not in the generated output.\n");
    }
    if (buf) {
      printf("%s\n", buf);
    }
    tlib_did_fail();
    nr_free(buf);
    return NR_FAILURE;
  }

  tlib_did_pass();
  nr_free(buf);
  return NR_SUCCESS;
}

nr_status_t tlib_pass_if_not_diff_f(const char* result_file,
                                    const char* expect_file,
                                    const char* transformation,
                                    int do_sort,
                                    int not_diff,
                                    const char* file,
                                    int line) {
  nr_status_t r;
  char cmdbuf[2048];

  snprintf(cmdbuf, sizeof(cmdbuf), "cat %s | %s | %s | diff -u %s -",
           result_file, transformation, do_sort ? " LC_ALL=C sort " : " cat",
           expect_file);

  r = tlib_pass_if_exec_f("compare logfile", cmdbuf, not_diff, file, line);
  if (r != NR_SUCCESS) {
    snprintf(cmdbuf, sizeof(cmdbuf), "cat %s | %s | %s > %s", result_file,
             transformation, do_sort ? " LC_ALL=C sort " : " cat", expect_file);
    printf("To regenerate the expected output, do:\n%s\n", cmdbuf);
  }
  return r;
}
