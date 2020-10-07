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

#if ZEND_MODULE_API_NO >= ZEND_5_6_X_API_NO
#include "Zend/zend_virtual_cwd.h"
#else /* PHP < 5.6 */
#include "tsrm_virtual_cwd.h"
#endif

#if defined(ZTS)
#include "TSRM.h"
#endif

#endif /* PHP_INCLUDES_HDR */
