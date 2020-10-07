/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>
#include <stdlib.h>

#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"
#include "util_syscalls.h"
#include "util_text.h"

#include "tlib_main.h"

typedef struct _test_text_state_t {
  nrobj_t* key_value_hash;
  int processor_state;
} test_text_state_t;

static void test_read_file(void) {
  char* result;
  char file_name[BUFSIZ];
  const char stimulus[] = "junk\n";
  FILE* fp;

  result = nr_read_file_contents(0, 0);
  tlib_pass_if_true("null file name", 0 == result, "result=%p", result);
  nr_free(result);

  result = nr_read_file_contents("/etc/motd_non_existant", 0);
  tlib_pass_if_true("non existant file", 0 == result, "result=%p", result);
  nr_free(result);

  result = nr_read_file_contents("/", 0);
  tlib_pass_if_true("directory", 0 == result, "result=%p", result);
  nr_free(result);

  snprintf(file_name, sizeof(file_name), "/tmp/fileXXXXXX");
  mkstemp(file_name);
  fp = fopen(file_name, "w");
  tlib_pass_if_true("tmpfile", 0 != fp, "fp=%p", fp);
  fwrite(stimulus, 1, sizeof(stimulus), fp);
  fclose(fp);

  result = nr_read_file_contents(file_name, 0);
  tlib_pass_if_true("legit filename", 0 != result, "result=%p", result);
  if (0 != result) {
    tlib_pass_if_true("leading null byte", 0 == result[0], "result[0]=%d",
                      result[0]);
  }
  nr_free(result);

  result = nr_read_file_contents(file_name, 1);
  tlib_pass_if_true("legit filename", 0 != result, "result=%p", result);
  if (0 != result) {
    tlib_pass_if_true("leading byte", stimulus[0] == result[0], "result[0]=%d",
                      result[0]);
    tlib_pass_if_true("leading byte", 0 == result[1], "result[1]=%d",
                      result[1]);
  }
  nr_free(result);

  result = nr_read_file_contents(file_name, 1 << 24);
  tlib_pass_if_true("legit filename", 0 != result, "result=%p", result);
  if (0 != result) {
    tlib_pass_if_true("expected contents", 0 == nr_strcmp(stimulus, result),
                      "stimulus=%s yet result=%s", stimulus, result);
  }
  nr_free(result);

  nr_unlink(file_name);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = 4, .state_size = sizeof(test_text_state_t)};

void test_main(void* p NRUNUSED) {
  test_read_file();
}
