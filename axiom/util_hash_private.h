/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions to generate hashes from arbitrary strings.
 */
#ifndef UTIL_HASH_PRIVATE_HDR
#define UTIL_HASH_PRIVATE_HDR

#include <stdint.h>

extern uint32_t nr_hash_md5_low32(const unsigned char md5[16]);

#endif /* UTIL_HASH_PRIVATE_HDR */
