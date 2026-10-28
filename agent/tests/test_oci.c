/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_main.h"
#include "tlib_php.h"

#include "php_agent.h"
#include "php_oci.h"

/*
 * Runs PHP that creates a resource in $name and gives it to the store.
 */
static void set_statement(const char* name,
                          const char* sql,
                          nr_string_len_t sql_len) {
  char* code = nr_formatf("$%s = fopen('php://memory', 'r');", name);
  char* expr = nr_formatf("$%s", name);
  zval* stmt;

  tlib_php_request_eval(code TSRMLS_CC);
  stmt = tlib_php_request_eval_expr(expr TSRMLS_CC);
  nr_php_oci_statement_sql_set(stmt, sql, sql_len);
  nr_php_zval_free(&stmt);
  nr_free(code);
  nr_free(expr);
}

static const char* get_statement(const char* name) {
  char* expr = nr_formatf("$%s", name);
  zval* stmt = tlib_php_request_eval_expr(expr TSRMLS_CC);
  const char* sql = nr_php_oci_statement_sql_get(stmt);

  nr_php_zval_free(&stmt);
  nr_free(expr);
  return sql;
}

static void test_statement_sql(TSRMLS_D) {
  zval* not_a_resource;

  tlib_php_request_start();

  /*
   * Bad arguments.
   */
  not_a_resource = tlib_php_request_eval_expr("'text'" TSRMLS_CC);
  nr_php_oci_statement_sql_set(NULL, "SELECT 1", 8);
  nr_php_oci_statement_sql_set(not_a_resource, "SELECT 1", 8);
  tlib_pass_if_null("a string has no SQL",
                    nr_php_oci_statement_sql_get(not_a_resource));
  tlib_pass_if_null("NULL has no SQL", nr_php_oci_statement_sql_get(NULL));
  tlib_pass_if_size_t_equal("nothing was saved", 0,
                            nr_php_oci_statements_count());
  nr_php_zval_free(&not_a_resource);

  /*
   * Set, get and replace.
   */
  set_statement("a", "SELECT 1 FROM dual", 18);
  tlib_pass_if_str_equal("the SQL is found", "SELECT 1 FROM dual",
                         get_statement("a"));
  set_statement("b", "SELECT 2 FROM dual", 18);
  tlib_pass_if_str_equal("another statement has its own SQL",
                         "SELECT 2 FROM dual", get_statement("b"));
  tlib_pass_if_str_equal("the first is unchanged", "SELECT 1 FROM dual",
                         get_statement("a"));
  tlib_pass_if_size_t_equal("two entries", 2, nr_php_oci_statements_count());

  /*
   * The length bounds the copy.
   */
  set_statement("c", "SELECT 3 FROM dual; DROP", 18);
  tlib_pass_if_str_equal("the SQL is cut at the length", "SELECT 3 FROM dual",
                         get_statement("c"));

  /*
   * NULL and empty SQL remove the entry.
   */
  set_statement("d", "SELECT 4 FROM dual", 18);
  set_statement("d", NULL, 0);
  tlib_pass_if_null("NULL SQL removes the entry", get_statement("d"));
  set_statement("e", "SELECT 5 FROM dual", 18);
  set_statement("e", "", 0);
  tlib_pass_if_null("empty SQL removes the entry", get_statement("e"));

  /*
   * A statement that is freed loses its entry when the next one is saved.
   */
  tlib_php_request_eval("unset($a);" TSRMLS_CC);
  tlib_pass_if_size_t_equal("the entry stays until the next save", 3,
                            nr_php_oci_statements_count());
  set_statement("f", "SELECT 6 FROM dual", 18);
  tlib_pass_if_size_t_equal("the entry of the freed statement was dropped", 3,
                            nr_php_oci_statements_count());
  tlib_pass_if_str_equal("the others are kept", "SELECT 2 FROM dual",
                         get_statement("b"));

  /*
   * The id of a freed resource is not given to a new resource, so a new
   * statement does not find the SQL of a freed one.
   */
  tlib_php_request_eval("$g = fopen('php://memory', 'r');" TSRMLS_CC);
  tlib_pass_if_null("a new statement has no SQL", get_statement("g"));

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("" PTSRMLS_CC);
  test_statement_sql(TSRMLS_C);
  tlib_php_engine_destroy(TSRMLS_C);
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};
