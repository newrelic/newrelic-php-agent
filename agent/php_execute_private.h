/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This header exposes php_execute.c internals for unit testing.
 */
#ifndef PHP_EXECUTE_PRIVATE_HDR
#define PHP_EXECUTE_PRIVATE_HDR

/*
 * Purpose: Enable monitoring on specific functions in the framework.
 */
typedef void (*nr_framework_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected library.
 */
typedef void (*nr_library_enable_fn_t)(TSRMLS_D);

/*
 * Purpose: Enable monitoring on specific functions for a detected vulnerability
 *          management package.
 */
typedef void (*nr_vuln_mgmt_enable_fn_t)();

typedef struct _nr_framework_table_t {
  const char* framework_name;
  const char* config_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_framework_special_fn_t special;
  nr_framework_enable_fn_t enable;
  nrframework_t detected;
} nr_framework_table_t;

typedef struct _nr_library_table_t {
  const char* library_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_library_enable_fn_t enable;
} nr_library_table_t;

typedef struct _nr_vuln_mgmt_table_t {
  const char* package_name;
  const char* file_to_check;
  size_t file_to_check_len;
  nr_vuln_mgmt_enable_fn_t enable;
} nr_vuln_mgmt_table_t;

extern const nr_framework_table_t all_frameworks[];
extern const int num_all_frameworks;

extern const nr_library_table_t libraries[];
extern const size_t num_libraries;

extern const nr_library_table_t logging_frameworks[];
extern const size_t num_logging_frameworks;

extern const nr_vuln_mgmt_table_t vuln_mgmt_packages[];
extern const size_t num_packages;

/*
 * Purpose : ONLY for testing to verify library/framework/logging-framework
 *           detection behavior directly, without going through
 *           nr_php_execute_file (which also executes the file's op array).
 *
 *           Detect library and framework usage from a PHP file. Enables a
 *           library or framework if the passed file is defined as a key
 *           file for this library or framework.
 *
 * Params  : 1. Full name of a PHP file.
 *           2. Length of the file name.
 */
extern void nr_php_user_instrumentation_from_file(const char* filename,
                                                  const size_t filename_len);

#endif /* PHP_EXECUTE_PRIVATE_HDR */
