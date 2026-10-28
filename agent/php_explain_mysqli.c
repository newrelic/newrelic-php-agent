/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions that actually generate an explain plan for
 * MySQLi queries.
 */
#include "php_agent.h"
#include "Zend/zend_exceptions.h"
#include "php_call.h"
#include "php_explain.h"
#include "php_explain_mysqli.h"
#include "php_hash.h"
#include "php_mysqli.h"
#include "fw_support.h"
#include "util_memory.h"
#include "util_strings.h"

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#include "ext/mysqli/php_mysqli_structs.h"
#endif

/*
 * Purpose : Iterator function to add a field to the columns in an explain
 *           plan.
 *
 * Params  : 1. The field to add, as a stdClass containing a "name" property.
 *           2. The explain plan.
 *           3. The hash key (which is ignored).
 *
 * Returns : ZEND_HASH_APPLY_KEEP.
 */
static int nr_php_explain_mysqli_add_field_to_plan(zval* field,
                                                   nr_explain_plan_t* plan,
                                                   zend_hash_key* key NRUNUSED
                                                       TSRMLS_DC);

/*
 * Purpose : Add the given fields to the columns in an explain plan.
 *
 * Params  : 1. An array of fields to add, as returned by
 *              mysqli_result::fetch_fields().
 *           2. The explain plan.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
static nr_status_t nr_php_explain_mysqli_add_fields_to_plan(
    zval* fields,
    nr_explain_plan_t* plan TSRMLS_DC);

/*
 * Purpose : Execute a mysqli_stmt.
 *
 * Params  : 1. The statement to execute.
 *           2. The binding params array,
 *              optionally null if nothing to bind
 *              or if already bound
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
static nr_status_t nr_php_explain_mysqli_execute(zval* stmt, zval* params TSRMLS_DC);

/*
 * Purpose : Retrieve the result of a mysqli_stmt that executed an EXPLAIN
 *           query and populate an explain plan.
 *
 * Params  : 1. The statement that contains the result set.
 *
 * Returns : A newly allocated explain plan, or NULL on error.
 */
static nr_explain_plan_t* nr_php_explain_mysqli_fetch_plan(
    zval* stmt TSRMLS_DC);

/*
 * Purpose : Helper function to duplicate a MySQLi link and issue an EXPLAIN
 *           query for the given SQL.
 *
 * Params  : 1. The link to duplicate.
 *           2. The handle of the MySQLi statement, or 0 if there was no
 *              statement.
 *           3. The SQL to explain.
 *           4. Params to be bound in execute
 *
 * Returns : A newly allocated explain plan, or NULL on error.
 */
static nr_explain_plan_t* nr_php_explain_mysqli_issue(
    nr_mysqli_metadata_link_handle_t link_handle,
    const zval* app_stmt,
    nr_php_object_handle_t handle,
    const char* sql,
    zval* params TSRMLS_DC);

/*
 * Purpose : Prepare an EXPLAIN query.
 *
 * Params  : 1. The MySQLi link.
 *           2. The query to EXPLAIN.
 *
 * Returns : A new mysqli_stmt object, or NULL on error.
 */
static zval* nr_php_explain_mysqli_prepare(zval* link,
                                           const char* query TSRMLS_DC);

nr_explain_plan_t* nr_php_explain_mysqli_query(const nrtxn_t* txn,
                                               zval* link,
                                               const char* sql,
                                               int sql_len,
                                               nrtime_t start,
                                               nrtime_t stop TSRMLS_DC) {
  nrtime_t duration;
  nr_explain_plan_t* plan = NULL;
  char* query;

  if ((NULL == txn) || (NULL == sql)) {
    return NULL;
  }

  if (!nr_php_mysqli_zval_is_link(link TSRMLS_CC)) {
    return NULL;
  }

  duration = nr_time_duration(start, stop);
  if (!nr_php_explain_wanted(txn, duration TSRMLS_CC)) {
    return NULL;
  }

  if (!nr_php_explain_mysql_query_is_explainable(sql, sql_len)) {
    return NULL;
  }

  query = nr_strndup(sql, sql_len);
  plan = nr_php_explain_mysqli_issue(
      (nr_mysqli_metadata_link_handle_t)Z_OBJ_HANDLE_P(link), NULL, 0, query,
      NULL TSRMLS_CC);
  nr_free(query);

  return plan;
}

/*
 * The number of parameters of a prepared statement, or 0 if it cannot be read.
 * The property is read here and not with nr_php_get_zval_object_property():
 * mysqli computes its properties and returns them in storage that belongs to
 * the caller.
 */
static long nr_php_explain_mysqli_stmt_param_count(const zval* stmt) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  zval rv;
  const zval* count;

  if (NULL != EG(exception)) {
    return 0;
  }

  count = zend_read_property(Z_OBJCE_P(stmt), ZVAL_OR_ZEND_OBJECT(stmt),
                             "param_count", sizeof("param_count") - 1, 1, &rv);

  return (count && (IS_LONG == Z_TYPE_P(count))) ? Z_LVAL_P(count) : 0;
#else
  (void)stmt;
  return 0;
#endif
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
/*
 * Whether the mysqli extension that is loaded was built on mysqlnd. The data
 * read below is mysqlnd's: the agent is compiled against the headers of one
 * PHP build and runs in the customer's, whose mysqli may use libmysqlclient
 * instead, with a different structure behind the same pointers.
 */
int nr_php_explain_mysqli_uses_mysqlnd(void) {
  const zend_module_entry* mysqli;
  const zend_module_dep* dep;

  mysqli = (const zend_module_entry*)zend_hash_str_find_ptr(
      &module_registry, "mysqli", sizeof("mysqli") - 1);
  if ((NULL == mysqli) || (NULL == mysqli->deps)) {
    return 0;
  }

  for (dep = mysqli->deps; dep->name; dep++) {
    if ((MODULE_DEP_REQUIRED == dep->type)
        && (0 == nr_strcmp(dep->name, "mysqlnd"))) {
      return 1;
    }
  }

  return 0;
}

/*
 * The mysqlnd data of a statement object, or NULL. mysqli binds the variables
 * of bind_param() into it, and holds them for as long as the statement lives.
 */
static const MYSQLND_STMT_DATA* nr_php_explain_mysqli_stmt_data(
    const zval* stmt) {
  const mysqli_object* intern;
  const MYSQLI_RESOURCE* resource;
  const MY_STMT* my_stmt;

  if (!nr_php_mysqli_zval_is_stmt(stmt)
      || !nr_php_explain_mysqli_uses_mysqlnd()) {
    return NULL;
  }

  intern = (const mysqli_object*)((const char*)Z_OBJ_P(stmt)
                                  - XtOffsetOf(mysqli_object, zo));
  resource = (const MYSQLI_RESOURCE*)intern->ptr;
  my_stmt = resource ? (const MY_STMT*)resource->ptr : NULL;

  return (my_stmt && my_stmt->stmt) ? my_stmt->stmt->data : NULL;
}

/* The number of variables that bind_param() bound. */
long nr_php_explain_mysqli_bound_count(const zval* stmt) {
  const MYSQLND_STMT_DATA* data = nr_php_explain_mysqli_stmt_data(stmt);

  return (data && data->param_bind) ? (long)data->param_count : 0;
}

/*
 * Purpose : Bind the values that bind_param() bound on the application's
 *           statement to the statement of the explain.
 *
 * Params  : 1. The statement of the explain.
 *           2. The application's statement.
 *
 * Returns : NR_SUCCESS, or NR_FAILURE if nothing was bound or the bind failed.
 *
 * Notes   : The values are copies: the explain does not hold the application's
 *           variables. The type letters come from the types that were bound.
 */
static nr_status_t nr_php_explain_mysqli_bind_live(zval* dest,
                                                   const zval* stmt) {
  const MYSQLND_STMT_DATA* data = nr_php_explain_mysqli_stmt_data(stmt);
  zval** bind_args;
  zval* retval;
  char* format;
  unsigned int count;
  unsigned int i;
  nr_status_t status = NR_FAILURE;

  if ((NULL == data) || (NULL == data->param_bind)
      || (0 == data->param_count)) {
    return NR_FAILURE;
  }

  count = data->param_count;
  bind_args = (zval**)nr_calloc(count + 1, sizeof(zval*));
  format = (char*)nr_malloc(count + 1);

  for (i = 0; i < count; i++) {
    const zval* bound = &data->param_bind[i].zv;
    zval copy;

    if (Z_ISREF_P(bound)) {
      bound = Z_REFVAL_P(bound);
    }

    switch (data->param_bind[i].type) {
      case MYSQL_TYPE_DOUBLE:
        format[i] = 'd';
        break;
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_LONG:
        format[i] = 'i';
        break;
      default:
        format[i] = 's';
        break;
    }

    /* bind_param() takes its arguments by reference. */
    ZVAL_COPY(&copy, bound);
    bind_args[i + 1] = nr_php_zval_alloc();
    ZVAL_NEW_REF(bind_args[i + 1], &copy);
  }
  format[count] = '\0';

  bind_args[0] = nr_php_zval_alloc();
  ZVAL_STRINGL(bind_args[0], format, count);

  retval = nr_php_call_user_func(dest, "bind_param", count + 1,
                                 bind_args TSRMLS_CC);
  if (nr_php_is_zval_true(retval)) {
    status = NR_SUCCESS;
  }

  for (i = 0; i <= count; i++) {
    nr_php_zval_free(&bind_args[i]);
  }
  nr_php_zval_free(&retval);
  nr_free(bind_args);
  nr_free(format);

  return status;
}
#endif

nr_explain_plan_t* nr_php_explain_mysqli_stmt(const nrtxn_t* txn,
                                              const zval* stmt,
                                              nr_php_object_handle_t handle,
                                              zval* params,
                                              nrtime_t start,
                                              nrtime_t stop TSRMLS_DC) {
  nrtime_t duration;
  zval* link = NULL;
  nr_mysqli_metadata_link_handle_t link_handle = 0;
  nr_explain_plan_t* plan = NULL;
  char* query = NULL;

  if ((NULL == txn)) {
    return NULL;
  }

  duration = nr_time_duration(start, stop);
  if (!nr_php_explain_wanted(txn, duration TSRMLS_CC)) {
    return NULL;
  }

  link = nr_php_mysqli_query_get_link(handle TSRMLS_CC);
  if (link) {
    link_handle = (nr_mysqli_metadata_link_handle_t)Z_OBJ_HANDLE_P(link);
    query = nr_php_mysqli_query_get_query(handle TSRMLS_CC);
  }

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  if (NULL == query) {
    const char* sql = nr_php_mysqli_statement_sql_get(stmt);

    /*
     * The transaction has no link or no query for the statement, for example
     * because it was prepared in an earlier transaction. The SQL and the link
     * saved with the statement object are used instead.
     */
    if ((NULL == sql)
        || (NR_FAILURE
            == nr_php_mysqli_statement_link_get(stmt, &link_handle))) {
      return NULL;
    }
    query = nr_strdup(sql);

    /*
     * Without the parameters the statement would fail to execute on the
     * explain connection, so it is not opened. The parameters are the ones
     * mysqli holds for the statement, if they can be read and there are as
     * many as the statement has.
     */
    if ((NULL == params) && !nr_php_mysqli_query_has_bind(handle TSRMLS_CC)
        && (nr_php_explain_mysqli_stmt_param_count(stmt)
            != nr_php_explain_mysqli_bound_count(stmt))) {
      nr_free(query);
      return NULL;
    }
  }
#else
  (void)stmt;
#endif

  if (NULL == query) {
    return NULL;
  }

  if (!nr_php_explain_mysql_query_is_explainable(query, nr_strlen(query))) {
    nr_free(query);
    return NULL;
  }

  plan = nr_php_explain_mysqli_issue(
      link_handle, stmt,
      nr_php_mysqli_query_exists(handle TSRMLS_CC) ? handle : 0, query,
      params TSRMLS_CC);
  nr_free(query);

  return plan;
}

static int nr_php_explain_mysqli_add_field_to_plan(zval* field,
                                                   nr_explain_plan_t* plan,
                                                   zend_hash_key* key NRUNUSED
                                                       TSRMLS_DC) {
  char* name;
  zval* name_zv = NULL;

  NR_UNUSED_TSRMLS;

  if (!nr_php_is_zval_valid_object(field)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  name_zv = nr_php_get_zval_object_property(field, "name" TSRMLS_CC);
  if (!nr_php_is_zval_valid_string(name_zv)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  name = nr_strndup(Z_STRVAL_P(name_zv), Z_STRLEN_P(name_zv));
  nr_explain_plan_add_column(plan, name);
  nr_free(name);

  return ZEND_HASH_APPLY_KEEP;
}

static nr_status_t nr_php_explain_mysqli_add_fields_to_plan(
    zval* fields,
    nr_explain_plan_t* plan TSRMLS_DC) {
  if ((NULL == fields) || (IS_ARRAY != Z_TYPE_P(fields)) || (NULL == plan)) {
    return NR_FAILURE;
  }

  nr_php_zend_hash_zval_apply(
      Z_ARRVAL_P(fields),
      (nr_php_zval_apply_t)nr_php_explain_mysqli_add_field_to_plan,
      plan TSRMLS_CC);

  return NR_SUCCESS;
}

static nr_status_t nr_php_explain_mysqli_execute(zval* stmt, zval* params TSRMLS_DC) {
  zval* retval;
  nr_status_t status;

  if (params) {
    retval = nr_php_call(stmt, "execute", params);
  } else {
    retval = nr_php_call(stmt, "execute");
  }
  status = nr_php_is_zval_true(retval) ? NR_SUCCESS : NR_FAILURE;

  nr_php_zval_free(&retval);

  return status;
}

static nr_explain_plan_t* nr_php_explain_mysqli_fetch_plan(
    zval* stmt TSRMLS_DC) {
  zval* bind_retval = NULL;
  zval* fields = NULL;
  zend_uint i;
  zend_uint num_fields = 0;
  nr_explain_plan_t* plan = NULL;
  zval* result = NULL;
  zval** results = NULL;

  /*
   * If everyone used mysqlnd, we could just call mysqli_stmt::get_result() here
   * and the code is pretty straightforward from there. Unfortunately,
   * libmysqlclients builds don't support this, so we use the
   * mysqli_stmt::bind_result() API to fetch rows.
   */
  result = nr_php_call(stmt, "result_metadata");
  if (NULL == result) {
    goto end;
  }

  fields = nr_php_call(result, "fetch_fields");
  plan = nr_explain_plan_create();
  if (NR_FAILURE
      == nr_php_explain_mysqli_add_fields_to_plan(fields, plan TSRMLS_CC)) {
    nr_explain_plan_destroy(&plan);
    goto end;
  }

  num_fields = nr_explain_plan_column_count(plan);
  if (num_fields <= 0) {
    nr_explain_plan_destroy(&plan);
    goto end;
  }

  /*
   * Set up the bindings. First, we have to allocate the zvals that will
   * receive results.
   */
  results = (zval**)nr_calloc(num_fields, sizeof(zval*));
  for (i = 0; i < num_fields; i++) {
    results[i] = nr_php_zval_alloc();
    nr_php_zval_prepare_out_arg(results[i]);
  }

  /*
   * Call mysqli_stmt::bind_result to bind our result variables.
   */
  bind_retval = nr_php_call_user_func(stmt, "bind_result", num_fields,
                                      results TSRMLS_CC);
  if (!nr_php_is_zval_true(bind_retval)) {
    nr_explain_plan_destroy(&plan);
    goto end;
  }

  /*
   * Actually call mysqli_stmt::fetch repeatedly to get the rows in the result
   * set.
   */
  while (1) {
    zval* fetch_retval = NULL;
    nrobj_t* plan_row = nro_new_array();

    fetch_retval = nr_php_call(stmt, "fetch");
    if (!nr_php_is_zval_true(fetch_retval)) {
      nr_php_zval_free(&fetch_retval);
      nro_delete(plan_row);
      break;
    }

    for (i = 0; i < num_fields; i++) {
      nr_php_explain_add_value_to_row(results[i], plan_row);
    }

    nr_explain_plan_add_row(plan, plan_row);
    nro_delete(plan_row);
    nr_php_zval_free(&fetch_retval);
  }

end:
  if (results) {
    for (i = 0; i < num_fields; i++) {
      nr_php_zval_free(&results[i]);
    }
    nr_free(results);
  }

  nr_php_zval_free(&bind_retval);
  nr_php_zval_free(&fields);
  nr_php_zval_free(&result);

  return plan;
}

static nr_explain_plan_t* nr_php_explain_mysqli_issue(
    nr_mysqli_metadata_link_handle_t link_handle,
    const zval* app_stmt,
    nr_php_object_handle_t handle,
    const char* sql,
    zval* params TSRMLS_DC) {
  int error_reporting;
  nr_php_user_error_handler_state_t user_error_handler;
  zval* link_dup = NULL;
  nr_explain_plan_t* plan = NULL;
  zval* stmt = NULL;

  /*
   * An exception is pending when the application's own call threw, for
   * example a failed query on PHP 8.1 and later. There is nothing to explain,
   * and no call into PHP runs while an exception is pending, so the explain
   * connection could not be created.
   */
  if (NULL != EG(exception)) {
    return NULL;
  }

#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO
  (void)app_stmt;
#endif

  error_reporting = nr_php_silence_errors(TSRMLS_C);

  /*
   * The application's error handler is not called while the explain connection
   * runs. A warning from that connection, such as a refused login, would call
   * it, and a handler can close the application's link: the data saved for the
   * link, which the explain reads through pointers while it calls PHP, would be
   * freed. It also keeps the handler from seeing warnings that the
   * application's own calls did not cause.
   */
  user_error_handler = nr_php_silence_user_error_handler(TSRMLS_C);
  NRTXNGLOBAL(generating_explain_plan) = 1;

  link_dup = nr_php_mysqli_link_duplicate_handle(link_handle TSRMLS_CC);
  if (NULL == link_dup) {
    goto end;
  }

  stmt = nr_php_explain_mysqli_prepare(link_dup, sql TSRMLS_CC);
  if (NULL == stmt) {
    goto end;
  }

  if (handle) {
    if (NR_FAILURE == nr_php_mysqli_query_rebind(handle, stmt TSRMLS_CC)) {
      goto end;
    }
  }
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  else if (app_stmt && (NULL == params)
           && (nr_php_explain_mysqli_bound_count(app_stmt) > 0)) {
    /*
     * The parameters were bound in an earlier transaction: mysqli still holds
     * the variables, and their values are what execute() used.
     */
    if (NR_FAILURE == nr_php_explain_mysqli_bind_live(stmt, app_stmt)) {
      goto end;
    }
  }
#endif

  if (NR_FAILURE == nr_php_explain_mysqli_execute(stmt, params TSRMLS_CC)) {
    goto end;
  }

  plan = nr_php_explain_mysqli_fetch_plan(stmt TSRMLS_CC);

end:
  /*
   * On PHP 8.1 and later mysqli throws mysqli_sql_exception when a call on
   * the explain connection fails, for example when the connection is refused.
   * That exception must not reach the application: clear any exception thrown
   * while the plan was generated.
   */
  if (NULL != EG(exception)) {
    zend_clear_exception();
  }

  /*
   * Destroying the duplicated link zval will take care of closing the
   * connection, as mysqli objects have a destructor that takes care of that,
   * and we don't increment the refcount on this variable anywhere else.
   */
  nr_php_zval_free(&link_dup);
  nr_php_zval_free(&stmt);

  nr_php_restore_user_error_handler(user_error_handler TSRMLS_CC);
  nr_php_restore_errors(error_reporting TSRMLS_CC);
  NRTXNGLOBAL(generating_explain_plan) = 0;

  return plan;
}

static zval* nr_php_explain_mysqli_prepare(zval* link,
                                           const char* query TSRMLS_DC) {
  char* explain_query = NULL;
  zval* query_zv = NULL;
  zval* stmt = NULL;

  explain_query = nr_formatf("EXPLAIN %s", query);
  query_zv = nr_php_zval_alloc();
  nr_php_zval_str(query_zv, explain_query);

  stmt = nr_php_call(link, "prepare", query_zv);
  if (!nr_php_mysqli_zval_is_stmt(stmt TSRMLS_CC)) {
    nr_php_zval_free(&stmt);
  }

  nr_php_zval_free(&query_zv);
  nr_free(explain_query);

  return stmt;
}
