/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_datastore.h"

#include "nr_datastore_instance.h"
#include "util_hashmap.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static void test_has_conn(TSRMLS_D) {
  nr_datastore_instance_t* instance;

  tlib_php_request_start();

  /*
   * Test : Invalid parameters.
   */
  tlib_pass_if_int_equal("NULL key", 0,
                         nr_php_datastore_has_conn(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_int_equal("blank key", 0,
                         nr_php_datastore_has_conn("" TSRMLS_CC));
  tlib_pass_if_int_equal("missing key", 0,
                         nr_php_datastore_has_conn("foo" TSRMLS_CC));

  instance = nr_datastore_instance_create("host", "port", "database");
  nr_hashmap_set(NRPRG(datastore_connections), NR_PSTR("foo"), instance);
  tlib_fail_if_int_equal("found key", 0,
                         nr_php_datastore_has_conn("foo" TSRMLS_CC));

  tlib_php_request_end();
}

static void test_instance_remove(TSRMLS_D) {
  nr_datastore_instance_t* instance;

  tlib_php_request_start();

  instance = nr_datastore_instance_create("host", "port", "database");
  nr_hashmap_set(NRPRG(datastore_connections), NR_PSTR("foo"), instance);

  /*
   * Test : Invalid parameters. In this case, we're just looking for not
   *        crashing and not altering the hashtable.
   */
  nr_php_datastore_instance_remove(NULL TSRMLS_CC);
  tlib_pass_if_size_t_equal("invalid parameters", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  /*
   * Test : Normal operation.
   */
  nr_php_datastore_instance_remove("" TSRMLS_CC);
  tlib_pass_if_size_t_equal("blank key", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_datastore_instance_remove("bar" TSRMLS_CC);
  tlib_pass_if_size_t_equal("missing key", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_datastore_instance_remove("foo" TSRMLS_CC);
  tlib_pass_if_size_t_equal("found key", 0,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_datastore_instance_remove("foo" TSRMLS_CC);
  tlib_pass_if_size_t_equal("duplicate call", 0,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  tlib_php_request_end();
}

static void test_instance_retrieve(TSRMLS_D) {
  nr_datastore_instance_t* instance;

  tlib_php_request_start();

  /*
   * Test : Invalid parameters.
   */
  tlib_pass_if_null("NULL key",
                    nr_php_datastore_instance_retrieve(NULL TSRMLS_CC));

  /*
   * Test : Normal operation.
   */
  tlib_pass_if_null("blank key",
                    nr_php_datastore_instance_retrieve("" TSRMLS_CC));
  tlib_pass_if_null("missing key",
                    nr_php_datastore_instance_retrieve("foo" TSRMLS_CC));

  instance = nr_datastore_instance_create("host", "port", "database");
  nr_hashmap_set(NRPRG(datastore_connections), NR_PSTR("foo"), instance);
  tlib_pass_if_ptr_equal("found key", instance,
                         nr_php_datastore_instance_retrieve("foo" TSRMLS_CC));

  tlib_php_request_end();
}

static void test_instance_save(TSRMLS_D) {
  nr_datastore_instance_t* a;
  nr_datastore_instance_t* b;

  tlib_php_request_start();
  a = nr_datastore_instance_create("host", "port", "database");
  b = nr_datastore_instance_create("different host", "port", "database");

  /*
   * Test : Invalid parameters. In this case, we're just looking for not
   *        crashing and not altering the hashtable.
   */
  nr_php_datastore_instance_save(NULL, NULL TSRMLS_CC);
  nr_php_datastore_instance_save("foo", NULL TSRMLS_CC);
  nr_php_datastore_instance_save(NULL, a TSRMLS_CC);

  tlib_pass_if_size_t_equal("invalid parameters", 0,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  /*
   * Test : Normal operation.
   */
  nr_php_datastore_instance_save("foo", a TSRMLS_CC);
  tlib_pass_if_ptr_equal(
      "set", a, nr_hashmap_get(NRPRG(datastore_connections), NR_PSTR("foo")));
  tlib_pass_if_size_t_equal("set", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  nr_php_datastore_instance_save("foo", b TSRMLS_CC);
  tlib_pass_if_ptr_equal(
      "overwrite", b,
      nr_hashmap_get(NRPRG(datastore_connections), NR_PSTR("foo")));
  tlib_pass_if_size_t_equal("overwrite", 1,
                            nr_hashmap_count(NRPRG(datastore_connections)));

  tlib_php_request_end();
}

static void test_make_key(TSRMLS_D) {
  char* expected;
  zval* invalid;
  char* key;
  zval* object;
  zval* resource;

  tlib_php_request_start();

  invalid = tlib_php_zval_create_default(IS_LONG TSRMLS_CC);
  object = tlib_php_zval_create_default(IS_OBJECT TSRMLS_CC);
  resource = tlib_php_zval_create_default(IS_RESOURCE TSRMLS_CC);

  tlib_pass_if_null("invalid connection",
                    nr_php_datastore_make_key(invalid, "foo"));

  key = nr_php_datastore_make_key(NULL, NULL);
  tlib_pass_if_str_equal("NULL connection and extension", "type=<NULL> id=0",
                         key);
  nr_free(key);

  key = nr_php_datastore_make_key(NULL, "foo");
  tlib_pass_if_str_equal("NULL connection", "type=foo id=0", key);
  nr_free(key);

  expected
      = nr_formatf("type=object id=%lu", (unsigned long)Z_OBJ_HANDLE_P(object));
  key = nr_php_datastore_make_key(object, "foo");
  tlib_pass_if_str_equal("object connection", expected, key);
  nr_free(expected);
  nr_free(key);

  expected
      = nr_formatf("type=resource id=%ld", nr_php_zval_resource_id(resource));
  key = nr_php_datastore_make_key(resource, "foo");
  tlib_pass_if_str_equal("resource connection", expected, key);
  nr_free(expected);
  nr_free(key);

  nr_php_zval_free(&invalid);
  nr_php_zval_free(&object);
  nr_php_zval_free(&resource);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  test_has_conn(TSRMLS_C);
  test_instance_remove(TSRMLS_C);
  test_instance_retrieve(TSRMLS_C);
  test_instance_save(TSRMLS_C);
  test_make_key(TSRMLS_C);

  tlib_php_engine_destroy(TSRMLS_C);
}
