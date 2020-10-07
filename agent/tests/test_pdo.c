/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_pdo.h"
#include "php_pdo_private.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

static zval* pdo_new(const char* dsn TSRMLS_DC) {
  zval* dsn_zv = nr_php_zval_alloc();
  zval* pdo = nr_php_zval_alloc();
  zend_class_entry* pdo_ce = nr_php_find_class("pdo" TSRMLS_CC);
  zval* retval = NULL;

  object_init_ex(pdo, pdo_ce);

  nr_php_zval_str(dsn_zv, dsn);
  retval = nr_php_call_user_func(pdo, "__construct", 1, &dsn_zv TSRMLS_CC);

  nr_php_zval_free(&dsn_zv);
  nr_php_zval_free(&retval);

  return pdo;
}

static zval* pdostatement_new(zval* pdo, const char* query TSRMLS_DC) {
  zval* query_zv = nr_php_zval_alloc();
  zval* stmt = NULL;

  nr_php_zval_str(query_zv, query);
  stmt = nr_php_call_user_func(pdo, "prepare", 1, &query_zv TSRMLS_CC);

  nr_php_zval_free(&query_zv);

  return stmt;
}

static void test_datastore_make_key(void) {
  pdo_dbh_t dbh;
  pdo_driver_t driver = {
      .driver_name = "mysql",
      .driver_name_len = 5,
  };
  char* key = NULL;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dbh", nr_php_pdo_datastore_make_key(NULL));

  /*
   * Test : Invalid pdo_dbh_t.
   */
  nr_memset(&dbh, 0, sizeof(dbh));

  dbh.driver = &driver;
  tlib_pass_if_null("NULL dbh.data_source",
                    nr_php_pdo_datastore_make_key(&dbh));

  dbh.data_source = "foo";
  tlib_pass_if_null("0 dbh.data_source_len",
                    nr_php_pdo_datastore_make_key(&dbh));

  /*
   * Test : Valid pdo_dbh_t.
   */
  dbh.data_source_len = 3;

  key = nr_php_pdo_datastore_make_key(&dbh);
  tlib_pass_if_str_equal("with driver", "type=pdo driver=mysql dsn=foo", key);
  nr_free(key);

  dbh.driver = NULL;
  key = nr_php_pdo_datastore_make_key(&dbh);
  tlib_pass_if_str_equal("without driver", "type=pdo driver=<NULL> dsn=foo",
                         key);
  nr_free(key);
}

static void test_get_database_object_from_object(TSRMLS_D) {
  pdo_dbh_t* dbh_pdo;
  pdo_dbh_t* dbh_stmt;
  size_t i;
  zval** invalid_zvals;
  zval* pdo;
  zval* stmt;

  tlib_php_request_start();

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL zval",
                    nr_php_pdo_get_database_object_from_object(NULL TSRMLS_CC));

  invalid_zvals = tlib_php_zvals_not_of_type(IS_OBJECT TSRMLS_CC);
  for (i = 0; invalid_zvals[i]; i++) {
    tlib_pass_if_null(
        "non-object zval",
        nr_php_pdo_get_database_object_from_object(invalid_zvals[i] TSRMLS_CC));
  }
  tlib_php_free_zval_array(&invalid_zvals);

  pdo = nr_php_zval_alloc();
  object_init(pdo);
  tlib_pass_if_null("non-PDO object zval",
                    nr_php_pdo_get_database_object_from_object(pdo TSRMLS_CC));
  nr_php_zval_free(&pdo);

  /*
   * Test : PDO.
   */
  pdo = pdo_new("sqlite::memory:" TSRMLS_CC);
  dbh_pdo = nr_php_pdo_get_database_object_from_object(pdo TSRMLS_CC);
  tlib_pass_if_not_null("PDO object", dbh_pdo);
  tlib_pass_if_str_equal("PDO object driver", "sqlite",
                         dbh_pdo->driver->driver_name);

  /*
   * Test : PDOStatement.
   */
  stmt = pdostatement_new(pdo, "SELECT * FROM SQLITE_MASTER" TSRMLS_CC);
  dbh_stmt = nr_php_pdo_get_database_object_from_object(stmt TSRMLS_CC);
  tlib_pass_if_ptr_equal("PDOStatement object", dbh_pdo, dbh_stmt);

  nr_php_zval_free(&stmt);
  nr_php_zval_free(&pdo);

  tlib_php_request_end();
}

static void test_get_datastore_for_driver(void) {
  size_t i;

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_int_equal("NULL driver", NR_DATASTORE_PDO,
                         nr_php_pdo_get_datastore_for_driver(NULL));

  /*
   * Test : Normal operation.
   */
  for (i = 0; i < (sizeof(nr_php_pdo_datastore_mappings)
                   / sizeof(nr_php_pdo_datastore_mapping_t));
       i++) {
    const nr_php_pdo_datastore_mapping_t* mapping
        = &nr_php_pdo_datastore_mappings[i];

    tlib_pass_if_int_equal(
        mapping->driver_name, mapping->datastore,
        nr_php_pdo_get_datastore_for_driver(mapping->driver_name));
  }
}

static void test_get_datastore_internal(void) {
  /*
   * The actual operation of nr_php_pdo_get_datastore_internal() is effectively
   * tested by other unit tests; this test serves to simply ensure that we
   * don't accidentally break the guarantee that it won't die if you give a
   * NULL dbh.
   */
  tlib_pass_if_int_equal("NULL dbh", NR_DATASTORE_PDO,
                         nr_php_pdo_get_datastore_internal(NULL));
}

static void test_get_driver_internal(void) {
  pdo_dbh_t dbh;
  pdo_driver_t null_driver = {
      .driver_name = NULL,
      .driver_name_len = 0,
  };
  pdo_driver_t valid_driver = {
      .driver_name = "mysql",
      .driver_name_len = 5,
  };

  /*
   * Test : Bad parameters.
   */
  tlib_pass_if_null("NULL dbh", nr_php_pdo_get_driver_internal(NULL));

  /*
   * Test : Invalid pdo_dbh_t.
   */
  nr_memset(&dbh, 0, sizeof(dbh));
  tlib_pass_if_null("NULL dbh.driver", nr_php_pdo_get_driver_internal(&dbh));

  dbh.driver = &null_driver;
  tlib_pass_if_null("NULL dbh.driver.driver_name",
                    nr_php_pdo_get_driver_internal(&dbh));

  /*
   * Test : Normal operation.
   */
  dbh.driver = &valid_driver;
  tlib_pass_if_str_equal("valid name", valid_driver.driver_name,
                         nr_php_pdo_get_driver_internal(&dbh));
}

static void test_disable_persistence(TSRMLS_D) {
  zval** bad_options;
  zval* expected;
  int i;
  zval* input;
  zval* output;
  zval* persistent;

  tlib_php_request_start();

  persistent = tlib_php_request_eval_expr("PDO::ATTR_PERSISTENT" TSRMLS_CC);

  /*
   * Test : Bad options.
   */
  tlib_pass_if_null("NULL options should fail to disable persistence",
                    nr_php_pdo_disable_persistence(NULL TSRMLS_CC));

  bad_options = tlib_php_zvals_not_of_type(IS_ARRAY TSRMLS_CC);
  for (i = 0; bad_options[i]; i++) {
    tlib_pass_if_null("non-array options should fail to disable persistence",
                      nr_php_pdo_disable_persistence(bad_options[i] TSRMLS_CC));
  }
  tlib_php_free_zval_array(&bad_options);

  /*
   * There's no useful way to test not having PDO or PDO::ATTR_PERSISTENT
   * available. We'll just assume those tests are appropriately defensive.
   */

  /*
   * Test : Empty options array.
   */
  input = tlib_php_request_eval_expr("array()" TSRMLS_CC);
  output = nr_php_pdo_disable_persistence(input TSRMLS_CC);
  tlib_pass_if_not_null(
      "an empty input array should return a valid output array", output);
  tlib_pass_if_zval_identical("the output array should match the input array",
                              input, output);
  nr_php_zval_free(&input);
  nr_php_zval_free(&output);

  /*
   * Test : Valid options array, but no PDO::ATTR_PERSISTENT entry.
   */
  input = tlib_php_request_eval_expr(
      "array("
      "PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,"
      ")" TSRMLS_CC);
  output = nr_php_pdo_disable_persistence(input TSRMLS_CC);
  tlib_pass_if_not_null(
      "a valid input array without a PDO::ATTR_PERSISTENT entry should return "
      "a valid output array",
      output);
  tlib_pass_if_zval_identical("the output array should match the input array",
                              input, output);
  nr_php_zval_free(&input);
  nr_php_zval_free(&output);

  /*
   * Test : Valid options arrays with only a PDO::ATTR_PERSISTENT entry.
   */
  for (i = 0; i < 2; i++) {
    expected = tlib_php_request_eval_expr(
        "array(PDO::ATTR_PERSISTENT => false)" TSRMLS_CC);

    if (i) {
      input = tlib_php_request_eval_expr(
          "array(PDO::ATTR_PERSISTENT => true)" TSRMLS_CC);
    } else {
      input = tlib_php_request_eval_expr(
          "array(PDO::ATTR_PERSISTENT => false)" TSRMLS_CC);
    }

    output = nr_php_pdo_disable_persistence(input TSRMLS_CC);
    tlib_pass_if_not_null(
        "a valid input array with a PDO::ATTR_PERSISTENT entry should "
        "return a valid output array ",
        output);
    tlib_pass_if_zval_identical(
        "the output array should match the expected array", expected, output);

    nr_php_zval_free(&expected);
    nr_php_zval_free(&input);
    nr_php_zval_free(&output);
  }

  /*
   * Test : Valid options arrays with possibly invalid PDO::ATTR_PERSISTENT
   *        entries (which should be converted to false anyway).
   */
  bad_options = tlib_php_zvals_of_all_types(TSRMLS_C);
  expected = tlib_php_request_eval_expr(
      "array(PDO::ATTR_PERSISTENT => false)" TSRMLS_CC);

  for (i = 0; bad_options[i]; i++) {
#ifdef PHP7
    if (IS_UNDEF == Z_TYPE_P(bad_options[i])) {
      continue;
    }
#endif

    input = nr_php_zval_alloc();

    array_init(input);
    nr_php_add_index_zval(input, (ulong)Z_LVAL_P(persistent), bad_options[i]);

    output = nr_php_pdo_disable_persistence(input TSRMLS_CC);
    tlib_pass_if_not_null(
        "a valid input array with a PDO::ATTR_PERSISTENT entry should "
        "return a valid output array ",
        output);
    tlib_pass_if_zval_identical(
        "the output array should match the expected array", expected, output);

    nr_php_zval_free(&input);
    nr_php_zval_free(&output);
  }
  nr_php_zval_free(&expected);
  tlib_php_free_zval_array(&bad_options);

  /*
   * Test : Valid options array with other keys that should be left alone.
   */
  input = tlib_php_request_eval_expr(
      "$options = array("
      "PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,"
      "PDO::ATTR_PERSISTENT => true,"
      ")" TSRMLS_CC);
  expected = tlib_php_request_eval_expr(
      "$options = array("
      "PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,"
      "PDO::ATTR_PERSISTENT => false,"
      ")" TSRMLS_CC);

  output = nr_php_pdo_disable_persistence(input TSRMLS_CC);
  tlib_pass_if_not_null(
      "a valid input array with a PDO::ATTR_PERSISTENT entry should "
      "return a valid output array ",
      output);
  tlib_pass_if_zval_identical(
      "the output array should match the expected array", expected, output);

  nr_php_zval_free(&expected);
  nr_php_zval_free(&input);
  nr_php_zval_free(&output);

  nr_php_zval_free(&persistent);
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && !defined(PHP7)
  void*** tsrm_ls = NULL;
#endif /* ZTS && !PHP7 */

  tlib_php_engine_create("" PTSRMLS_CC);

  if (tlib_php_require_extension("PDO" TSRMLS_CC)) {
    if (tlib_php_require_extension("pdo_sqlite" TSRMLS_CC)) {
      test_datastore_make_key();
      test_get_database_object_from_object(TSRMLS_C);
      test_get_datastore_for_driver();
      test_get_datastore_internal();
      test_get_driver_internal();
    }

    test_disable_persistence(TSRMLS_C);
  }

  tlib_php_engine_destroy(TSRMLS_C);
}
