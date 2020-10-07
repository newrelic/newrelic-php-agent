/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/stat.h>

#include <stddef.h>
#include <stdio.h>

#include "util_memory.h"
#include "util_syscalls.h"
#include "util_text.h"

static inline size_t size_min(size_t a, size_t b) {
  return (a < b) ? a : b;
}

char* nr_read_file_contents(const char* file_name, size_t max_bytes) {
  int stat_status;
  struct stat statbuf;
  char* file_contents = NULL;
  size_t bytes_to_read = 0;
  size_t bytes_allocated = 0;
  size_t offset;
  FILE* fp = 0;

  if (0 == file_name) {
    return NULL;
  }

  stat_status = nr_stat(file_name, &statbuf);
  if (stat_status < 0) {
    return NULL;
  }

  if (0 == S_ISREG(statbuf.st_mode)) {
    return NULL;
  }

  fp = fopen(file_name, "r");
  if (0 == fp) {
    return NULL;
  }

  bytes_to_read = size_min(statbuf.st_size, max_bytes);
  bytes_allocated = bytes_to_read + 1;
  file_contents = (char*)nr_malloc(bytes_allocated);
  offset = 0;
  while (bytes_to_read > 0) {
    const int nread
        = fread(file_contents + offset, sizeof(char), bytes_to_read, fp);

    if (nread < 0) {
      break;
    }
    offset += nread;
    bytes_to_read -= nread;
  }
  file_contents[bytes_allocated - 1] = 0;
  fclose(fp);
  fp = NULL;
  return file_contents;
}
