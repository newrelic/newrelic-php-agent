/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file is a helper utility for New Relic install.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(__GNUC__)
#define NRUNUSED __attribute__((__unused__))
#else
#define NRUNUSED
#endif

#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define PATH_MAX MAXPATHLEN
#else
#define PATH_MAX 2048
#endif
#endif

typedef int (*cmdfunc_t)(int argc, char* const argv[]);

static char* getrp(const char* fn) {
  static char fnb[PATH_MAX];
  char* r;

  r = realpath(fn, fnb);
  return r;
}

static int do_stat(int argc NRUNUSED, char* const argv[]) {
  struct stat sb;
  int r = stat(argv[2], &sb);

  if (0 != r) {
    return 1;
  }
  printf("%lld\n", (long long)sb.st_size);
  return 0;
}

static int do_realpath(int argc NRUNUSED, char* const argv[]) {
  char* rp = getrp(argv[2]);
  if (0 == rp) {
    return 1;
  }
  puts(rp);
  return 0;
}

struct _ucmd_t {
  const char* cmd;
  cmdfunc_t fn;
} cmds[] = {{"stat", do_stat}, {"realpath", do_realpath}, {0, 0}};

static void usage(void) {
  printf(
      "Usage: newrelic-iutil {stat filename} | {realpath filename} | "
      "{version}\n");
  exit(2);
}

int main(int argc, char* const argv[]) {
  int i;

  if (2 == argc) {
    if (0 == strcmp(argv[1], "-V")) {
      puts(NR_VERSION);
      exit(0);
    }
    usage();
  }

  if (argc <= 2) {
    usage();
  }

  for (i = 0; 0 != cmds[i].cmd; i++) {
    if (0 == strcmp(argv[1], cmds[i].cmd)) {
      return cmds[i].fn(argc, argv);
    }
  }

  usage();
  return 0;
}
