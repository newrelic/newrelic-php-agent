/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file provides utility functions for generating explain plans.
 */
#include "php_agent.h"
#include "php_explain.h"
#include "php_explain_pdo_mysql.h"
#include "php_pdo.h"
#include "nr_segment_datastore.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

nr_status_t nr_php_explain_add_value_to_row(const zval* zv, nrobj_t* row) {
  if ((NULL == zv) || (NULL == row)) {
    return NR_FAILURE;
  }

  nr_php_zval_unwrap(zv);

  /*
   * All we need to do is add the value to the row object, but we want to make
   * sure it's the most accurate type when we sent it to the collector, hence
   * the switch on the zval type.
   */
  switch (Z_TYPE_P(zv)) {
    case IS_LONG:
      nro_set_array_long(row, 0, Z_LVAL_P(zv));
      break;

    case IS_DOUBLE:
      nro_set_array_double(row, 0, Z_DVAL_P(zv));
      break;

    case IS_STRING: {
      char* str;

      if (!nr_php_is_zval_valid_string(zv)) {
        nrl_verbosedebug(NRL_SQL, "%s: invalid string value", __func__);
        nro_set_array_string(row, 0, "Unknown value");
        break;
      }

      /*
       * The double string duplication that effectively occurs here is
       * unfortunate, but nrobj_t can't deal with unterminated strings, so
       * here we are.
       */
      str = (char*)nr_zalloc(Z_STRLEN_P(zv) + 1);

      nr_strxcpy(str, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
      nro_set_array_string(row, 0, str);

      nr_free(str);
    } break;

    case IS_NULL:
      nro_set_array_none(row, 0);
      break;

#ifdef PHP7
    case IS_TRUE:
      nro_set_array_boolean(row, 0, 1);
      break;

    case IS_FALSE:
      nro_set_array_boolean(row, 0, 0);
      break;
#else
    case IS_BOOL:
      nro_set_array_boolean(row, 0, Z_BVAL_P(zv));
      break;
#endif

    default:
      nrl_verbosedebug(NRL_SQL, "%s: unknown zval type %d", __func__,
                       Z_TYPE_P(zv));
      nro_set_array_string(row, 0, "Unknown value");
      break;
  }

  return NR_SUCCESS;
}

int nr_php_explain_mysql_query_is_explainable(const char* query, int length) {
  /*
   * Per http://dev.mysql.com/doc/refman/5.5/en/using-explain.html, only SELECT
   * queries can be EXPLAINed.
   *
   * This does not handle preceding whitespace and comments.
   */
  const char* prefix = "SELECT ";
  int prefix_len = sizeof("SELECT ") - 1;
  int idx;

  if (NULL == query) {
    return 0;
  }

  if (length < prefix_len) {
    return 0;
  }

  if (0 != nr_strnicmp(query, prefix, prefix_len)) {
    return 0;
  }

  /*
   * We do not want to perform an explain query if this SQL is actually
   * multiple separated queries.  Doing so would be very unsafe if the
   * second statement has side effects, like an UPDATE or a DELETE.
   * Rather than parse the SQL, we simply look for a semicolon in anything
   * other than the last character. This is overly simplistic: the semicolon
   * could be in a comment or a string. Nonetheless, it is defensive.
   *
   * nr_strchr cannot be used since this query string may not be NUL terminated.
   */
  idx = nr_strnidx(query, ";", length);
  if ((idx >= 0) && (idx < (length - 1))) {
    return 0;
  }

  /*
   * MySQL supports locking reads via special suffixes that can be applied to
   * SELECT queries. These can cause deadlocks if we issue an EXPLAIN query on
   * another connection, so we'll blacklist those suffixes.
   *
   * As with the above check, this may be prone to false positives if these
   * strings are contained within a comment or string literal in the query, but
   * it's not worth the extra effort to avoid that. Being defensive is the
   * important part.
   *
   * Docs: http://dev.mysql.com/doc/refman/5.7/en/innodb-locking-reads.html
   */
  if (nr_strncaseidx(query, " FOR UPDATE", length) >= 0) {
    return 0;
  }
  if (nr_strncaseidx(query, " LOCK IN SHARE MODE", length) >= 0) {
    return 0;
  }

  return 1;
}

nr_explain_plan_t* nr_php_explain_pdo_statement(nrtxn_t* txn,
                                                zval* stmt,
                                                zval* parameters,
                                                nrtime_t start,
                                                nrtime_t stop TSRMLS_DC) {
  nrtime_t duration;
  nr_explain_plan_t* plan = NULL;

  if ((NULL == txn) || (NULL == stmt)) {
    return NULL;
  }

  duration = nr_time_duration(start, stop);
  if (!nr_php_explain_wanted(txn, duration TSRMLS_CC)) {
    return NULL;
  }

  if (!nr_php_object_instanceof_class(stmt, "PDOStatement" TSRMLS_CC)) {
    return NULL;
  }

  /*
   * When drivers other than MySQL are supported, they should be added below.
   */
  if (0
      == nr_strncmp(nr_php_pdo_get_driver(stmt TSRMLS_CC), NR_PSTR("mysql"))) {
    nrtime_t explain_start;
    nrtime_t explain_stop;

    NRTXNGLOBAL(generating_explain_plan) = 1;
    explain_start = nr_get_time();

    plan = nr_php_explain_pdo_mysql_statement(stmt, parameters TSRMLS_CC);

    explain_stop = nr_get_time();
    NRTXNGLOBAL(generating_explain_plan) = 0;

    /*
     * Fire off a supportability metric so we can figure out if the overhead of
     * explain plans is problematic.
     */
    nrm_force_add(txn->unscoped_metrics,
                  "Supportability/DatabaseUtils/Calls/explain_plan",
                  nr_time_duration(explain_start, explain_stop));
  }

  return plan;
}

int nr_php_explain_wanted(const nrtxn_t* txn, nrtime_t duration TSRMLS_DC) {
  return ((0 == NRTXNGLOBAL(generating_explain_plan))
          && nr_segment_potential_explain_plan(txn, duration));
}
