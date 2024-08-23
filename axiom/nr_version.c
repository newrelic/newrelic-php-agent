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
 *   rose               20Dec2023 (10.15)
 *   snapdragon         23Jan2024 (10.16)
 *   tulip              21Feb2024 (10.17)
 *   ulmus              04Mar2024 (10.18)
 *   viburnum           18Mar2024 (10.19)
 *   wallflower         06May2024 (10.20)
 *   xerophyllum        20May2024 (10.21)
 *   yarrow             26Jun2024 (10.22)
 *   zinnia             30Jul2024 (11.0)
 */
#define NR_CODENAME "amethyst"

const char* nr_version(void) {
  return NR_STR2(NR_VERSION);
}

const char* nr_version_verbose(void) {
  return NR_STR2(NR_VERSION) " (\"" NR_CODENAME
                             "\" - \"" NR_STR2(NR_COMMIT) "\")";
}
