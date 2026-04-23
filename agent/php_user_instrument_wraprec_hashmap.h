/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_USER_INSTRUMENT_WRAPREC_HASHMAP_HDR
#define PHP_USER_INSTRUMENT_WRAPREC_HASHMAP_HDR

#include "php_user_instrument.h"

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO

// Forward declarations for hashmap types
typedef struct _nr_func_hashmap nr_func_hashmap_t;
typedef struct _nr_scope_hashmap nr_scope_hashmap_t;

// clang-format off

extern void nr_php_user_instrument_wraprec_hashmap_init(void);
extern nruserfn_t* nr_php_user_instrument_wraprec_hashmap_add(const char* namestr, size_t namestrlen);
extern nruserfn_t* nr_php_user_instrument_wraprec_hashmap_get(zend_string *func_name, zend_string *scope_name);
extern void nr_php_user_instrument_wraprec_hashmap_destroy(void);

extern void nr_php_user_instrument_wraprec_hashmap_ini_init(void);
extern nruserfn_t* nr_php_user_instrument_wraprec_hashmap_ini_add(const char* namestr, size_t namestrlen);
extern void nr_php_user_instrument_wraprec_hashmap_ini_destroy(void);
extern void nr_php_user_instrument_wraprec_hashmap_replay_ini(void);

// clang-format on

#endif /* ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO */

#endif
