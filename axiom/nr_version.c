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
 * Current version naming scheme is flowers
 *
 *   vanilla            07Dec2020 (9.15)
 *   watermelon         25Jan2021 (9.16)
 *   xigua              21Apr2021 (9.17)
 *   yam                23Aug2021 (9.18)
 *   zomp               02Mar2022 (9.19)
 *   allium             14Mar2022 (9.20)
 *   buttercup          26Apr2022 (9.21)
 *   cosmos             29Jun2022 (10.0)
 *   dahlia             19Sep2022 (10.1)
 *   echinacea          03Oct2022 (10.2)
 *   freesia            03Nov2022 (10.3)
 *   goldenrod          12Dec2022 (10.4)
 */
#define NR_CODENAME "hydrangea"

const char* nr_version(void) {
  return NR_STR2(NR_VERSION);
}

const char* nr_version_verbose(void) {
  return NR_STR2(NR_VERSION) " (\"" NR_CODENAME
                             "\" - \"" NR_STR2(NR_COMMIT) "\")";
}
