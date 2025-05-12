/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_USER_INSTRUMENT_WRAPREC_HASHMAP_HDR
#define PHP_USER_INSTRUMENT_WRAPREC_HASHMAP_HDR

#include "php_user_instrument.h"

// clang-format off

extern void nr_php_user_instrument_wraprec_hashmap_init(void);
extern nruserfn_t* nr_php_user_instrument_wraprec_hashmap_add(const char* namestr, size_t namestrlen, bool *is_new_wraprec_ptr);
extern nruserfn_t* nr_php_user_instrument_wraprec_hashmap_get(zend_string *func_name, zend_string *scope_name);
extern void nr_php_user_instrument_wraprec_hashmap_destroy(void);

// clang-format on

#endif
