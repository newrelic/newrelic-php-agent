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
 *   yam                23Aug2021 (9.18)
 *   zomp               02Mar2022 (9.19)
 *   allium             14Mar2022 (9.20)
 *   buttercup          26Apr2022 (9.21)
 *   cosmos             29Jun2022 (10.0)
 *   dahlia             19Sep2022 (10.1)
 *   echinacea          03Oct2022 (10.2)
 *   freesia            03Nov2022 (10.3)
 *   goldenrod          12Dec2022 (10.4)
 *   hydrangea          18Jan2023 (10.5)
 *   impatiens          13Feb2023 (10.6)
 *   jasmine            08Mar2023 (10.7)
 *   kalmia             27Mar2023 (10.8)
 *   lilac              05Apr2023 (10.9)
 *   marigold           30May2023 (10.10)
 *   narcissus          20Jun2023 (10.11)
 *   orchid             20Sep2023 (10.12)
 *   poinsettia         03Oct2023 (10.13)
 *   quince             13Nov2023 (10.14)
 */
#define NR_CODENAME "rose"

const char* nr_version(void) {
  return NR_STR2(NR_VERSION);
}

const char* nr_version_verbose(void) {
  return NR_STR2(NR_VERSION) " (\"" NR_CODENAME
                             "\" - \"" NR_STR2(NR_COMMIT) "\")";
}
