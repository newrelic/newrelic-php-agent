/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "lib_php_amqplib.h"
#include "fw_support.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO > ZEND_7_1_X_API_NO

static void declare_php_amqplib_package_class(const char* ns,
                                              const char* klass,
                                              const char* package_version) {
  char* source = nr_formatf(
      "namespace %s;"
      "class %s{"
      "const VERSION = '%s';"
      "}",
      ns, klass, package_version);

  tlib_php_request_eval(source);

  nr_free(source);
}

static void test_nr_lib_php_amqplib_handle_version(void) {
#define LIBRARY_NAME "php-amqplib/php-amqplib"
  const char* library_versions[]
      = {"7", "10", "100", "4.23", "55.34", "6123.45", "0.4.5"};
  nr_php_package_t* p = NULL;
#define TEST_DESCRIPTION_FMT                                                  \
  "nr_lib_php_amqplib_handle_version with library_versions[%ld]=%s: package " \
  "major version metric - %s"
  char* test_description = NULL;
  size_t i = 0;

  /*
   * If lib_php_amqplib_handle_version function is ever called, we have already
   * detected the php-amqplib library.
   */

  /*
   * PhpAmqpLib/Package class exists. Should create php-amqplib package metric
   * suggestion with version
   */
  for (i = 0; i < sizeof(library_versions) / sizeof(library_versions[0]); i++) {
    tlib_php_request_start();

    declare_php_amqplib_package_class("PhpAmqpLib", "Package",
                                      library_versions[i]);
    nr_php_amqplib_handle_version();

    p = nr_php_packages_get_package(
        NRPRG(txn)->php_package_major_version_metrics_suggestions,
        LIBRARY_NAME);

    test_description = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                                  "suggestion created");
    tlib_pass_if_not_null(test_description, p);
    nr_free(test_description);

    test_description = nr_formatf(TEST_DESCRIPTION_FMT, i, library_versions[i],
                                  "suggested version set");
    tlib_pass_if_str_equal(test_description, library_versions[i],
                           p->package_version);
    nr_free(test_description);

    tlib_php_request_end();
  }

  /*
   * PhpAmqpLib/Package class does not exist, should create package metric
   * suggestion with PHP_PACKAGE_VERSION_UNKNOWN version. This case should never
   * happen in real situations.
   */
  tlib_php_request_start();

  nr_php_amqplib_handle_version();

  p = nr_php_packages_get_package(
      NRPRG(txn)->php_package_major_version_metrics_suggestions, LIBRARY_NAME);

  tlib_pass_if_not_null(
      "nr_lib_php_amqplib_handle_version when PhpAmqpLib\\Package class is not "
      "defined - "
      "suggestion created",
      p);
  tlib_pass_if_str_equal(
      "nr_lib_php_amqplib_handle_version when PhpAmqpLib\\Package class is not "
      "defined - "
      "suggested version set to PHP_PACKAGE_VERSION_UNKNOWN",
      PHP_PACKAGE_VERSION_UNKNOWN, p->package_version);

  tlib_php_request_end();

  /*
   * PhpAmqpLib\\Package class exists but VERSION does not.
   * Should create package metric suggestion with PHP_PACKAGE_VERSION_UNKNOWN
   * version. This case should never happen in real situations.
   */
  tlib_php_request_start();

  char* source
      = "namespace PhpAmqpLib;"
        "class Package{"
        "const SADLY_DEPRECATED = 5.4;"
        "}";

  tlib_php_request_eval(source);

  nr_php_amqplib_handle_version();

  p = nr_php_packages_get_package(
      NRPRG(txn)->php_package_major_version_metrics_suggestions, LIBRARY_NAME);

  tlib_pass_if_not_null(
      "nr_lib_php_amqplib_handle_version when PhpAmqpLib\\Package class is SET "
      "but the const VERSION does not exist - "
      "suggestion created",
      p);
  tlib_pass_if_str_equal(
      "nr_lib_php_amqplib_handle_version when PhpAmqpLib\\Package class is SET "
      "but the const VERSION does not exist - "
      "defined - "
      "suggested version set to PHP_PACKAGE_VERSION_UNKNOWN",
      PHP_PACKAGE_VERSION_UNKNOWN, p->package_version);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  test_nr_lib_php_amqplib_handle_version();
  tlib_php_engine_destroy();
}
#else
void test_main(void* p NRUNUSED) {}
#endif
