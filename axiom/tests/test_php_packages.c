/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_php_packages.h"
#include "tlib_main.h"
#include "util_memory.h"

static void test_php_package_create_destroy(void) {
  nr_php_package_t* package;
  nr_php_package_t* null_package = NULL;

  // Test: create new package and ensure it contains correct information
  package = nr_php_package_create("Laravel", "8.83.27");

  tlib_pass_if_not_null("create package", package);
  tlib_pass_if_str_equal("test package name", "Laravel", package->package_name);
  tlib_pass_if_str_equal("test package version", "8.83.27",
                         package->package_version);

  nr_php_package_destroy(package);

  // Test: passing NULL pointer should not cause crash
  nr_php_package_destroy(null_package);
  nr_php_package_destroy(NULL);
}

static void test_php_adding_packages_to_hashmap(void) {
  nr_php_package_t* package1;
  nr_php_package_t* package2;
  nr_php_package_t* package3;
  nr_php_packages_t* hm = nr_php_packages_create();
  int count;

  // Test: create multiple new packages and add to hashmap
  package1 = nr_php_package_create("Package One", "10.1.0");
  package2 = nr_php_package_create("Package Two", "11.2.0");
  package3 = nr_php_package_create("Package Three", "12.3.0");
  /* Should not crash: */

  nr_php_packages_add_package(NULL, package1);
  nr_php_packages_add_package(hm, NULL);
  nr_php_packages_add_package(hm, package1);
  nr_php_packages_add_package(hm, package2);
  nr_php_packages_add_package(hm, package3);

  count = nr_php_packages_count(hm);

  tlib_pass_if_int_equal("package count", 3, count);

  nr_php_packages_destroy(&hm);
  tlib_pass_if_null("PHP packages hashmap destroyed", hm);
}

static void test_php_package_to_json(void) {
  char* json;
  nr_php_package_t* package1;

  // Test: convert package to json
  package1 = nr_php_package_create("TestPackage", "7.2.0");
  // Ensure passing NULL does not cause crash
  json = nr_php_package_to_json(NULL);
  json = nr_php_package_to_json(package1);
  tlib_pass_if_str_equal("valid package", "[\"TestPackage\",\"7.2.0\",{}]",
                         json);
  nr_free(json);
  nr_php_package_destroy(package1);
}

static void test_php_packages_to_json_buffer(void) {
  nrbuf_t* buf = nr_buffer_create(0, 0);
  nr_php_packages_t* collection = nr_php_packages_create();
  nr_php_package_t* package1;
  nr_php_package_t* package2;
  nr_php_package_t* package3;
  nr_php_package_t* package4;
  nr_php_package_t* package5;
  int count;

  package1 = nr_php_package_create("Package One", "1.0.0");
  // Add package with same key, but different value. Newer value will be kept
  package2 = nr_php_package_create("Package One", "11.0");
  package3 = nr_php_package_create("Package Two", "2.0.0");
  // Add package with same key and same value. No action will happen
  package4 = nr_php_package_create("Package Two", "2.0.0");
  // Ensure passing NULL as the version does not cause crash and adds it to the
  // collection as a empty string with a space
  package5 = nr_php_package_create("Package Three", NULL);


  nr_php_packages_add_package(collection, package1);
  nr_php_packages_add_package(collection, package2);
  nr_php_packages_add_package(collection, package3);
  nr_php_packages_add_package(collection, package5);

  tlib_pass_if_int_equal("package count", 3, count);

  // Ensure passing NULL does not cause crash
  nr_php_packages_to_json_buffer(NULL, NULL);
  nr_php_packages_to_json_buffer(collection, NULL);
  nr_php_packages_to_json_buffer(NULL, buf);
  // Test: adding packages to buffer
  tlib_pass_if_bool_equal("filled collection bool check", true,
                          nr_php_packages_to_json_buffer(collection, buf));

  nr_buffer_add(buf, NR_PSTR("\0"));
  tlib_pass_if_str_equal("filled collection",
                         "[[\"Package One\",\"11.0\",{}],[\"Package "
                         "Three\",\" \",{}],[\"Package Two\",\"2.0.0\",{}]]",
                         nr_buffer_cptr(buf));
  nr_php_packages_destroy(&collection);
  nr_buffer_destroy(&buf);
}

static void test_php_packages_to_json(void) {
  char* json;
  nr_php_packages_t* h = nr_php_packages_create();
  nr_php_package_t* package1;
  nr_php_package_t* package2;
  nr_php_package_t* package3;

  // Test: passing NULL does not crash
  tlib_pass_if_null("NULL package", nr_php_packages_to_json(NULL));

  // Test: convert all packages in hashmap to json
  package1 = nr_php_package_create("Package One", "10.1.0");
  package2 = nr_php_package_create("Package Two", "11.2.0");
  // Ensure passing NULL as the version does not cause crash and adds it to the
  // collection as a empty string with a space
  package3 = nr_php_package_create("Package Three", NULL);

  nr_php_packages_add_package(h, package1);
  nr_php_packages_add_package(h, package2);
  nr_php_packages_add_package(h, package3);

  json = nr_php_packages_to_json(h);

  tlib_pass_if_str_equal("full hashmap",
                         "[[\"Package One\",\"10.1.0\",{}],[\"Package "
                         "Three\",\" \",{}],[\"Package Two\",\"11.2.0\",{}]]",
                         json);

  nr_free(json);
  nr_php_packages_destroy(&h);
}

static void test_php_package_exists_in_hashmap(void) {
  nr_php_package_t* package1;
  nr_php_package_t* package2;
  nr_php_packages_t* hm = nr_php_packages_create();
  int exists;

  // Test: check if package exists in hashmap
  package1 = nr_php_package_create("Package One", "10.1.0");
  package2 = nr_php_package_create("Package Two", "11.2.0");

  nr_php_packages_add_package(hm, package1);
  nr_php_packages_add_package(hm, package2);

  exists = nr_php_packages_has_package(hm, package1->package_name,
                                       nr_strlen(package1->package_name));

  tlib_pass_if_int_equal("package exists", 1, exists);

  nr_php_packages_destroy(&hm);
}

static void test_php_package_without_version(void) {
  char* json;
  nr_php_package_t* package1;
  nr_php_package_t* package2;
  nr_php_packages_t* hm = nr_php_packages_create();

  // Test: Passing NULL as the version does not cause crash and adds it to the
  // hashmap as a empty string with a space
  package1 = nr_php_package_create("Package One", NULL);
  package2 = nr_php_package_create("Package Two", NULL);

  nr_php_packages_add_package(hm, package1);
  nr_php_packages_add_package(hm, package2);
  json = nr_php_packages_to_json(hm);

  tlib_pass_if_str_equal(
      "full hashmap", "[[\"Package One\",\" \",{}],[\"Package Two\",\" \",{}]]",
      json);

  nr_free(json);
  nr_php_packages_destroy(&hm);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_php_package_create_destroy();
  test_php_adding_packages_to_hashmap();
  test_php_package_to_json();
  test_php_packages_to_json_buffer();
  test_php_packages_to_json();
  test_php_package_exists_in_hashmap();
  test_php_package_without_version();
}