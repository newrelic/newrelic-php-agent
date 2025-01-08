/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This header file exists for one primary reason: to wrap all of the PHP
 * include files in a file where we can use the GCC pragma to indicate that
 * this is a "system" include file. It eliminates a lot of useless warnings
 * from the PHP include files.
 */
#ifndef PHP_INCLUDES_HDR
#define PHP_INCLUDES_HDR

#ifdef __GNUC__
#pragma GCC system_header
#endif

#include "zend.h"
#include "zend_API.h"
#include "zend_builtin_functions.h"
#include "zend_compile.h"
#include "zend_errors.h"
#include "zend_execute.h"
#include "zend_extensions.h"
#include "zend_hash.h"
#include "zend_interfaces.h"
#include "zend_operators.h"
#include "zend_types.h"
#include "zend_vm.h"

#include "SAPI.h"

#include "php.h"
#include "php_ini.h"
#include "php_main.h"
#include "php_version.h"

#include "ext/pdo/php_pdo_driver.h"
#include "ext/standard/info.h"

/*
 * Zend Engine API numbers.
 * Find these numbers at: php-src/Zend/zend_modules.h
 */
#define ZEND_5_3_X_API_NO 20090626
#define ZEND_5_4_X_API_NO 20100525
#define ZEND_5_5_X_API_NO 20121212
#define ZEND_5_6_X_API_NO 20131226
#define ZEND_7_0_X_API_NO 20151012
#define ZEND_7_1_X_API_NO 20160303
#define ZEND_7_2_X_API_NO 20170718
#define ZEND_7_3_X_API_NO 20180731
#define ZEND_7_4_X_API_NO 20190902
#define ZEND_8_0_X_API_NO 20200930
#define ZEND_8_1_X_API_NO 20210902
#define ZEND_8_2_X_API_NO 20220829
#define ZEND_8_3_X_API_NO 20230831
#define ZEND_8_4_X_API_NO 20240924

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO /* PHP8+ */
#include "Zend/zend_observer.h"
#endif

#if ZEND_MODULE_API_NO >= ZEND_5_6_X_API_NO
#include "Zend/zend_virtual_cwd.h"
#else /* PHP < 5.6 */
#include "tsrm_virtual_cwd.h"
#endif

#if defined(ZTS)
#include "TSRM.h"
#endif

/*
 * The TSRMLS_* functions included below have actually been voided out since PHP
 * 7.0.  They were formally removed in PHP 8.0 but we still support in our code,
 * so we need to add the voided out macros here.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#define TSRMLS_D void
#define TSRMLS_DC
#define TSRMLS_C
#define TSRMLS_CC
#define TSRMLS_FETCH()
#endif

/*
 * The convert_to_explicit_type() macro was removed for 8.1.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
#define convert_to_explicit_type(pzv, type) \
  do {                                      \
    switch (type) {                         \
      case IS_NULL:                         \
        convert_to_null(pzv);               \
        break;                              \
      case IS_LONG:                         \
        convert_to_long(pzv);               \
        break;                              \
      case IS_DOUBLE:                       \
        convert_to_double(pzv);             \
        break;                              \
      case _IS_BOOL:                        \
        convert_to_boolean(pzv);            \
        break;                              \
      case IS_ARRAY:                        \
        convert_to_array(pzv);              \
        break;                              \
      case IS_OBJECT:                       \
        convert_to_object(pzv);             \
        break;                              \
      case IS_STRING:                       \
        convert_to_string(pzv);             \
        break;                              \
      default:                              \
        assert(0);                          \
        break;                              \
    }                                       \
  } while (0);
#endif

#endif /* PHP_INCLUDES_HDR */
