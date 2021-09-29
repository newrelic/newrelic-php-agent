/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file handles PDO MySQL explain plans.
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_explain.h"
#include "php_explain_pdo_mysql.h"
#include "php_hash.h"
#include "php_pdo.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/* {{{ Prototypes for static functions */

/*
 * Purpose : Iterator function to add the given key to the list of columns in
 *           an explain plan.
 *
 * Params  : 1. The field value (ignored).
 *           2. The explain plan to add columns to.
 *           3. The column name.
 *
 * Returns : ZEND_HASH_KEY_APPLY (to continue iteration).
 */
static int add_column_to_explain_plan(zval* value NRUNUSED,
                                      nr_explain_plan_t* plan,
                                      zend_hash_key* hash_key TSRMLS_DC);

/*
 * Purpose : Iterator function to add the given row to an explain plan.
 *
 * Params  : 1. The row array.
 *           2. The explain plan.
 *           3. The row index (ignored).
 *
 * Returns : ZEND_HASH_KEY_APPLY (to continue iteration).
 */
static int add_row_to_explain_plan(zval* row,
                                   nr_explain_plan_t* plan,
                                   zend_hash_key* hash_key NRUNUSED TSRMLS_DC);

/*
 * Purpose : Iterator function to add the given field value to a row in an
 *           explain plan.
 *
 * Params  : 1. The field value.
 *           2. The row object.
 *           3. The field name (ignored).
 *
 * Returns : ZEND_HASH_KEY_APPLY (to continue iteration).
 */
static int add_value_to_explain_plan(zval* value,
                                     nrobj_t* row,
                                     zend_hash_key* hash_key NRUNUSED
                                         TSRMLS_DC);

/*
 * Purpose : Given a PDOStatement that has been executed, retrieves the explain
 *           output and creates an nr_explain_plan_t structure that represents
 *           it.
 *
 * Params  : 1. The PDOStatement object.
 *
 * Returns : A new nr_explain_plan_t structure. The caller will need to destroy
 *           this when it's no longer required.
 */
static nr_explain_plan_t* fetch_explain_plan_from_stmt(zval* stmt TSRMLS_DC);

/*
 * Purpose : Prepares and executes an EXPLAIN query for the given statement.
 *
 * Params  : 1. The PDO object to issue the EXPLAIN query on.
 *           2. The original PDOStatement object.
 *           3. The parameters to bind to the EXPLAIN statement, or NULL to
 *              reuse those bound to the original PDOStatement object.
 *
 * Returns : A PDOStatement object that can be fetched from on success, or NULL
 *           on failure.
 */
static zval* issue_explain_query(zval* dbh,
                                 zval* original_stmt,
                                 zval* parameters TSRMLS_DC);

/*
 * Purpose : Silences PDO error reporting.
 *
 * Params  : 1. The PDO object.
 *
 * Returns : The previous error reporting mode.
 */
static enum pdo_error_mode set_pdo_silent(zval* dbh TSRMLS_DC);

/* }}} */

nr_explain_plan_t* nr_php_explain_pdo_mysql_statement(zval* stmt,
                                                      zval* parameters
                                                          TSRMLS_DC) {
  zval* dbh = NULL;
  zval* dup = NULL;
  zval* explain_stmt = NULL;
  pdo_stmt_t* pdo_stmt = NULL;
  nr_explain_plan_t* plan = NULL;

  pdo_stmt = nr_php_pdo_get_statement_object(stmt TSRMLS_CC);
  if (NULL == pdo_stmt) {
    nrl_verbosedebug(NRL_SQL,
                     "%s: unable to retrieve pdo_stmt_t from PDOStatement",
                     __func__);
    goto end;
  }

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
  if (!nr_php_explain_mysql_query_is_explainable(
          ZSTR_VAL(pdo_stmt->query_string), ZSTR_LEN(pdo_stmt->query_string))) {
#else
  if (!nr_php_explain_mysql_query_is_explainable(pdo_stmt->query_string,
                                                 pdo_stmt->query_stringlen)) {
#endif
    goto end;
  }

  dbh = &pdo_stmt->database_object_handle;
  dup = nr_php_pdo_duplicate(dbh TSRMLS_CC);
  if (NULL == dup) {
    goto end;
  }
  set_pdo_silent(dup TSRMLS_CC);

  explain_stmt = issue_explain_query(dup, stmt, parameters TSRMLS_CC);
  if (NULL == explain_stmt) {
    goto end;
  }

  plan = fetch_explain_plan_from_stmt(explain_stmt TSRMLS_CC);
  nr_php_zval_free(&explain_stmt);

end:
  nr_php_zval_free(&dup);
  nr_php_zval_free(&explain_stmt);

  return plan;
}

static int add_column_to_explain_plan(zval* value NRUNUSED,
                                      nr_explain_plan_t* plan,
                                      zend_hash_key* hash_key TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  if (nr_php_zend_hash_key_is_numeric(hash_key)) {
    nrl_verbosedebug(NRL_SQL, "%s: Unexpected non-string column name",
                     __func__);
    return ZEND_HASH_APPLY_KEEP;
  }

  nr_explain_plan_add_column(plan, nr_php_zend_hash_key_string_value(hash_key));

  return ZEND_HASH_APPLY_KEEP;
}

static int add_row_to_explain_plan(zval* row,
                                   nr_explain_plan_t* plan,
                                   zend_hash_key* hash_key NRUNUSED TSRMLS_DC) {
  nrobj_t* plan_row = NULL;

  if (!nr_php_is_zval_valid_array(row)) {
    nrl_verbosedebug(
        NRL_SQL,
        "%s: PDOStatement::fetchAll did not return a 2 dimensional array",
        __func__);
    return ZEND_HASH_APPLY_KEEP;
  }

  /*
   * If this is the first row in the result set, we need to add the columns to
   * the explain plan before we can add the row itself.
   */
  if (0 == nr_explain_plan_column_count(plan)) {
    nr_php_zend_hash_zval_apply(Z_ARRVAL_P(row),
                                (nr_php_zval_apply_t)add_column_to_explain_plan,
                                plan TSRMLS_CC);
  }

  /*
   * Now we iterate over the values in the row and add them to an nrobj_t that
   * we then add to the explain plan.
   */
  plan_row = nro_new_array();
  nr_php_zend_hash_zval_apply(Z_ARRVAL_P(row),
                              (nr_php_zval_apply_t)add_value_to_explain_plan,
                              plan_row TSRMLS_CC);
  nr_explain_plan_add_row(plan, plan_row);
  nro_delete(plan_row);

  return ZEND_HASH_APPLY_KEEP;
}

static int add_value_to_explain_plan(zval* value,
                                     nrobj_t* row,
                                     zend_hash_key* hash_key NRUNUSED
                                         TSRMLS_DC) {
  NR_UNUSED_TSRMLS;

  nr_php_explain_add_value_to_row(value, row);
  return ZEND_HASH_APPLY_KEEP;
}

static nr_explain_plan_t* fetch_explain_plan_from_stmt(zval* stmt TSRMLS_DC) {
  zval* data = NULL;
  zval* fetch_mode = NULL;
  nr_explain_plan_t* plan = NULL;

  /*
   * It's important that we control the fetch mode, for two reasons: firstly,
   * having the result set as a set of associative arrays makes the walking
   * logic much easier, and secondly, we want to prevent a fetch mode that can
   * instantiate objects from being used.
   */
  fetch_mode = nr_php_zval_alloc();
  ZVAL_LONG(fetch_mode, PDO_FETCH_ASSOC);

  /*
   * Explain plans should always be small enough that calling fetchAll won't be
   * too much of a drain on memory.
   */
  data = nr_php_call(stmt, "fetchAll", fetch_mode);
  if (!nr_php_is_zval_valid_array(data)) {
    nrl_verbosedebug(NRL_SQL, "%s: PDOStatement::fetchAll returned non-array",
                     __func__);
    goto end;
  }

  if (0 == zend_hash_num_elements(Z_ARRVAL_P(data))) {
    nrl_verbosedebug(NRL_SQL, "%s: PDOStatement::fetchAll returned empty array",
                     __func__);
    goto end;
  }

  /*
   * Basically, we want to walk over each row in the returned result set and
   * add it to the explain plan, using the keys from the first row to populate
   * the explain plan's columns.
   */
  plan = nr_explain_plan_create();
  nr_php_zend_hash_zval_apply(Z_ARRVAL_P(data),
                              (nr_php_zval_apply_t)add_row_to_explain_plan,
                              plan TSRMLS_CC);

end:
  nr_php_zval_free(&data);
  nr_php_zval_free(&fetch_mode);

  return plan;
}

static zval* issue_explain_query(zval* dbh,
                                 zval* original_stmt,
                                 zval* parameters TSRMLS_DC) {
  char* explain_query = NULL;
  zval* explain_stmt = NULL;
  pdo_stmt_t* pdo_stmt = NULL;

  if (NULL == original_stmt) {
    goto end;
  }

  pdo_stmt = nr_php_pdo_get_statement_object(original_stmt TSRMLS_CC);
  if (NULL == pdo_stmt) {
    nrl_verbosedebug(NRL_SQL, "%s: unable to get pdo_stmt_t from object",
                     __func__);
    goto end;
  }

  /*
   * Construct the EXPLAIN query that needs to be sent, which simply involves
   * prepending the keyword EXPLAIN.
   */
  /*
   * PHP 8.1 changed to using a zend_string
   */
  explain_query = (char*)nr_zalloc(
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
      ZSTR_LEN(pdo_stmt->query_string)
#else
      pdo_stmt->query_stringlen
#endif
      + sizeof("EXPLAIN "));
  nr_strcat(explain_query, "EXPLAIN ");
  nr_strncat(explain_query,
#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO
             ZSTR_VAL(pdo_stmt->query_string), ZSTR_LEN(pdo_stmt->query_string)
#else
             pdo_stmt->query_string, pdo_stmt->query_stringlen
#endif
                 );
  explain_stmt = nr_php_pdo_prepare_query(dbh, explain_query TSRMLS_CC);
  if (NULL == explain_stmt) {
    goto end;
  }

  /*
   * If the user didn't provide the bound parameters to
   * PDOStatement::execute(), it's crucial that we rebind the parameters from
   * the original query, as we've prepared a new query. (PDO doesn't require
   * drivers to support modification of the query in an existing prepared
   * statement, so we can't simply reuse the original statement object.)
   */
  if (NULL == parameters) {
    nr_php_pdo_rebind_parameters(original_stmt, explain_stmt TSRMLS_CC);
  }

  if (NR_FAILURE
      == nr_php_pdo_execute_query(explain_stmt, parameters TSRMLS_CC)) {
    nr_php_zval_free(&explain_stmt);
    goto end;
  }

end:
  nr_free(explain_query);
  return explain_stmt;
}

static enum pdo_error_mode set_pdo_silent(zval* dbh TSRMLS_DC) {
  enum pdo_error_mode mode = PDO_ERRMODE_SILENT;
  pdo_dbh_t* pdo_dbh = nr_php_pdo_get_database_object(dbh TSRMLS_CC);

  if (NULL != pdo_dbh) {
    mode = pdo_dbh->error_mode;
    pdo_dbh->error_mode = PDO_ERRMODE_SILENT;
  }

  return mode;
}
