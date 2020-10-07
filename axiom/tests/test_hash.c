/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stddef.h>
#include <stdio.h>

#include "util_hash.h"
#include "util_hash_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_text.h"

#include "tlib_main.h"

#define PATH_HASHING_TESTS_FILE CROSS_AGENT_TESTS_DIR "/cat/path_hashing.json"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 2, .state_size = 0};

#define tlib_pass_if_md5_equal(M, EXPECTED, ACTUAL)                        \
  {                                                                        \
    char hex[33] = {'\0'};                                                 \
    int i;                                                                 \
                                                                           \
    for (i = 0; i < 16; i++) {                                             \
      (void)snprintf(hex + (i * 2), 3, "%02x", (unsigned int)(ACTUAL)[i]); \
    }                                                                      \
                                                                           \
    tlib_pass_if_str_equal(M, EXPECTED, hex);                              \
  }

static void test_cat_path(void) {
  char* hash = NULL;
  char* refer = NULL;

  /*
   * The below inputs and outputs are taken from the CAT spec examples.
   */

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL txn_name", nr_hash_cat_path(NULL, "app", NULL));
  tlib_pass_if_null("NULL app_name", nr_hash_cat_path("txn", NULL, NULL));

  /*
   * Test : NULL referring path hash.
   */
  hash = nr_hash_cat_path("test", "23547", NULL);
  tlib_pass_if_str_equal("NULL referring", "1bd0ddbd", hash);
  refer = nr_strdup(hash);
  nr_free(hash);

  /*
   * Test : Invalid referring path hash should be treated as NULL.  This
   *        behavior is dictated by the cat_map.json cross agent tests in test
   *        "new_cat_corrupt_path_hash" as of 0f93ade.
   */
  hash = nr_hash_cat_path("test", "23547", "ZXYQEDABC");
  tlib_pass_if_str_equal("invalid referring path hash", "1bd0ddbd", hash);
  nr_free(hash);

  /*
   * Test : Compounded hash.
   */
  hash = nr_hash_cat_path("test", "23547", refer);
  tlib_pass_if_str_equal("double hash", "2c7166c7", hash);
  nr_free(hash);
  nr_free(refer);

  /*
   * Test : App and transaction names that cause the high bit of the MD5 to be
   *        set.
   */
  hash = nr_hash_cat_path("txn", "app", NULL);
  tlib_pass_if_str_equal("high MD5", "b95be233", hash);
  nr_free(hash);
}

static void test_cat_path_cross_agent(void) {
  int i;
  char* json;
  nrobj_t* tests;

  json = nr_read_file_contents(PATH_HASHING_TESTS_FILE, 10 * 1000 * 1000);
  tlib_pass_if_not_null(PATH_HASHING_TESTS_FILE "readable", json);
  tests = nro_create_from_json(json);
  nr_free(json);

  for (i = 1; i <= nro_getsize(tests); i++) {
    const char* app_name;
    const char* expected_result;
    const char* referring_path_hash;
    const char* test_name;
    const char* txn_name;
    char* result;
    const nrobj_t* test = nro_get_array_hash(tests, i, NULL);

    app_name = nro_get_hash_string(test, "applicationName", NULL);
    expected_result = nro_get_hash_string(test, "expectedPathHash", NULL);
    referring_path_hash = nro_get_hash_string(test, "referringPathHash", NULL);
    test_name = nro_get_hash_string(test, "name", NULL);
    txn_name = nro_get_hash_string(test, "transactionName", NULL);

    result = nr_hash_cat_path(txn_name, app_name, referring_path_hash);
    tlib_pass_if_str_equal(test_name, expected_result, result);

    nr_free(result);
  }

  nro_delete(tests);
}

static void test_md5(void) {
  unsigned char md5[16];

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_status_failure("NULL result", nr_hash_md5(NULL, NR_PSTR("")));
  tlib_pass_if_status_failure("NULL input", nr_hash_md5(md5, NULL, 0));
  tlib_pass_if_status_failure("negative size", nr_hash_md5(md5, "", -1));

  /*
   * Test : Normal usage.
   */
  tlib_pass_if_status_success("empty string", nr_hash_md5(md5, NR_PSTR("")));
  tlib_pass_if_md5_equal("empty string", "d41d8cd98f00b204e9800998ecf8427e",
                         md5);

  tlib_pass_if_status_success("non-empty string",
                              nr_hash_md5(md5, NR_PSTR("foobar")));
  tlib_pass_if_md5_equal("non-empty string", "3858f62230ac3c915f300c664312c63f",
                         md5);
}

static void test_md5_low32(void) {
  unsigned char md5[16];

  /*
   * Test : Not crashing on NULL.
   */
  (void)nr_hash_md5_low32(NULL);

  /*
   * Test : Normal usage.
   */
  (void)nr_hash_md5(md5, NR_PSTR(""));
  tlib_pass_if_uint32_t_equal("empty string", 0xecf8427e,
                              nr_hash_md5_low32(md5));

  (void)nr_hash_md5(md5, NR_PSTR("foobar"));
  tlib_pass_if_uint32_t_equal("non-empty string", 0x4312c63f,
                              nr_hash_md5_low32(md5));
}

static void test_mkhash(void) {
  unsigned int ui1;
  unsigned int ui2;
  int l;

  /*
   * Test 1: Parameter validation.
   */
  ui1 = nr_mkhash(0, 0);
  tlib_pass_if_true("NULL string hashes to 0", 0 == ui1, "hash=0x%x", ui1);
  ui1 = nr_mkhash("", 0);
  tlib_pass_if_true("empty string hashes to 0", 0 == ui1, "hash=0x%x", ui1);

  /*
   * Test 2: Hash with and without length
   */
  ui1 = nr_mkhash("abc", 0);
  tlib_pass_if_true("simple hash not 0", 0 != ui1, "hash=0x%x", ui1);
  l = 3;
  ui2 = nr_mkhash("abc", &l);
  tlib_pass_if_true("simple hash not 0", 0 != ui2, "hash=0x%x", ui2);
  tlib_pass_if_true("hashes match", ui1 == ui2, "hash1=0x%x hash2=0x%x", ui1,
                    ui2);

  /*
   * Test 3: Length computed correctly
   */
  l = 0;
  ui1 = nr_mkhash("abcdef", &l);
  tlib_pass_if_true("hash not 0", (0 != ui1) && (6 == l), "hash=0x%x, l=%d",
                    ui1, (int)l);
}

void test_main(void* p NRUNUSED) {
  test_cat_path();
  test_cat_path_cross_agent();
  test_md5();
  test_md5_low32();
  test_mkhash();
}
