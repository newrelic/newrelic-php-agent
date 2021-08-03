/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"
#include "nr_version.h"

/*
 * NR_VERSION ultimately comes from the top-level VERSION file.
 */
#ifndef NR_VERSION
#define NR_VERSION "unreleased"
#endif

/*
 * NR_COMMIT ultimately comes from the command $(git rev-parse HEAD)
 */
#ifndef NR_COMMIT
#define NR_COMMIT ""
#endif

/*
 * Current version naming scheme is color names that are also food.
 * Here's a list:
 *  https://en.wikipedia.org/wiki/List_of_colors_(compact)
 *
 *   ube                29Oct2020 (9.14)
 *   vanilla            07Dec2020 (9.15)
 *   watermelon         25Jan2021 (9.16)
 *   xigua              21Apr2021 (9.17)
 */
#define NR_CODENAME "yam"

const char* nr_version(void) {
  return NR_STR2(NR_VERSION);
}

const char* nr_version_verbose(void) {
  return NR_STR2(NR_VERSION) " (\"" NR_CODENAME
                             "\" - \"" NR_STR2(NR_COMMIT) "\")";
}
