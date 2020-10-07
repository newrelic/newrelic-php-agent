/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_VERSION_HDR
#define NR_VERSION_HDR

/*
 * Purpose : Returns the version number.
 *
 * Returns : A string representing the version number.
 */
extern const char* nr_version(void);

/*
 * Purpose : Returns exhaustive version information.
 *
 * Returns : A string representing the version information.
 */
extern const char* nr_version_verbose(void);

#endif /* NR_VERSION_HDR */
