/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file provides utility functions for handling PDO and PDOStatement
 * objects.
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_datastore.h"
#include "php_explain.h"
#include "php_hash.h"
#include "php_pdo.h"
#include "php_pdo_mysql.h"
#include "php_pdo_pgsql.h"
#include "util_hashmap.h"
#include "util_logging.h"
#include "util_strings.h"

#include "php_pdo_private.h"

nr_status_t nr_php_pdo_execute_query(zval* stmt, zval* parameters TSRMLS_DC) {
  zval* result = NULL;
  nr_status_t state = NR_FAILURE;

  if (parameters) {
    result = nr_php_call(stmt, "execute", parameters);
    if (NULL == result) {
      nrl_verbosedebug(
          NRL_SQL, "%s: error calling PDOStatement::execute with parameters",
          __func__);
      return NR_FAILURE;
    }
  } else {
    result = nr_php_call(stmt, "execute");
    if (NULL == result) {
      nrl_verbosedebug(NRL_SQL, "%s: error calling PDOStatement::execute",
                       __func__);
      return NR_FAILURE;
    }
  }

  if (!nr_php_is_zval_true(result)) {
    zval* error_info = NULL;

    /*
     * Try to get more detailed error information via
     * PDOStatement::errorInfo().
     */
    error_info = nr_php_call(stmt, "errorInfo");
    if (nr_php_is_zval_valid_array(error_info)) {
      const zval* message = NULL;

      /*
       * errorInfo() returns an array, but we're only really interested in
       * the error message, which is field 2.
       */
      message = nr_php_zend_hash_index_find(Z_ARRVAL_P(error_info), 2);
      if (nr_php_is_zval_valid_string(message)) {
        nrl_verbosedebug(
            NRL_SQL, "%s: PDOStatement::execute failed with error %.*s",
            __func__, (int)Z_STRLEN_P(message), Z_STRVAL_P(message));
        nr_php_zval_free(&error_info);
        goto end;
      }
    }

    /*
     * At this point, options are exhausted
     */
    nrl_verbosedebug(NRL_SQL,
                     "%s: PDOStatement::execute failed, and no error "
                     "information is available",
                     __func__);
    nr_php_zval_free(&error_info);
    goto end;
  }

  state = NR_SUCCESS;

end:
  nr_php_zval_free(&result);

  return state;
}

zval* nr_php_pdo_prepare_query(zval* dbh, const char* query TSRMLS_DC) {
  zval* query_zv = nr_php_zval_alloc();
  zval* stmt = NULL;

  nr_php_zval_str(query_zv, query);

  stmt = nr_php_call(dbh, "prepare", query_zv);
  if (!nr_php_object_instanceof_class(stmt, "PDOStatement" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_SQL, "%s: prepare did not return a PDOStatement",
                     __func__);

    nr_php_zval_free(&query_zv);
    nr_php_zval_free(&stmt);

    return NULL;
  }

  nr_php_zval_free(&query_zv);
  return stmt;
}

void nr_php_pdo_rebind_parameters(zval* source, zval* destination TSRMLS_DC) {
  pdo_stmt_t* pdo_stmt = nr_php_pdo_get_statement_object(source TSRMLS_CC);

  /*
   * For each parameter, we want to call bindParam with the original zval (so
   * that by-reference parameters are correctly handled).
   */
  if ((NULL != pdo_stmt) && (NULL != pdo_stmt->bound_params)) {
    nr_php_zend_hash_ptr_apply(
        pdo_stmt->bound_params,
        (nr_php_ptr_apply_t)nr_php_pdo_rebind_apply_parameter,
        destination TSRMLS_CC);
  }
}

int nr_php_pdo_rebind_apply_parameter(struct pdo_bound_param_data* param,
                                      zval* stmt,
                                      zend_hash_key* hash_key TSRMLS_DC) {
  zval* key = nr_php_zval_alloc();
  zval* value;
  zval* type = nr_php_zval_alloc();
  zval* retval = NULL;

#ifdef PHP7
  value = &param->parameter;
#else
  value = param->parameter;
#endif /* PHP7 */

  if (nr_php_zend_hash_key_is_string(hash_key)) {
    /*
     * String keys require no munging, and can be reused.
     */
    nr_php_zval_str_len(key, nr_php_zend_hash_key_string_value(hash_key),
                        nr_php_zend_hash_key_string_len(hash_key));
  } else {
    /*
     * PDOStatement::bindParam() expects numeric keys to be 1-indexed, but
     * they're actually stored 0-indexed in the pdo_stmt_t structure.
     */
    ZVAL_LONG(key, nr_php_zend_hash_key_integer(hash_key) + 1);
  }

  ZVAL_LONG(type, param->param_type);

  retval = nr_php_call(stmt, "bindParam", key, value, type);

  nr_php_zval_free(&key);
  nr_php_zval_free(&retval);
  nr_php_zval_free(&type);

  return ZEND_HASH_APPLY_KEEP;
}

pdo_dbh_t* nr_php_pdo_get_database_object_from_object(zval* obj TSRMLS_DC) {
  if (NULL == obj) {
    return NULL;
  }

  if (nr_php_object_instanceof_class(obj, "PDO" TSRMLS_CC)) {
    return nr_php_pdo_get_database_object_internal(obj TSRMLS_CC);
  } else if (nr_php_object_instanceof_class(obj, "PDOStatement" TSRMLS_CC)) {
    pdo_stmt_t* stmt = nr_php_pdo_get_statement_object_internal(obj TSRMLS_CC);

    if (NULL == stmt) {
      return NULL;
    }
    return stmt->dbh;
  }

  return NULL;
}

pdo_dbh_t* nr_php_pdo_get_database_object(zval* dbh TSRMLS_DC) {
  if (nr_php_object_instanceof_class(dbh, "PDO" TSRMLS_CC)) {
    return nr_php_pdo_get_database_object_internal(dbh TSRMLS_CC);
  }

  return NULL;
}

pdo_stmt_t* nr_php_pdo_get_statement_object(zval* stmt TSRMLS_DC) {
  if (nr_php_object_instanceof_class(stmt, "PDOStatement" TSRMLS_CC)) {
    return nr_php_pdo_get_statement_object_internal(stmt TSRMLS_CC);
  }

  return NULL;
}

const char* nr_php_pdo_get_driver_internal(pdo_dbh_t* dbh) {
  if (NULL == dbh) {
    return NULL;
  }

  if (NULL == dbh->driver) {
    nrl_verbosedebug(NRL_SQL, "%s: PDO driver is NULL", __func__);
    return NULL;
  }

  return dbh->driver->driver_name;
}

const char* nr_php_pdo_get_driver(zval* obj TSRMLS_DC) {
  pdo_dbh_t* dbh = NULL;

  dbh = nr_php_pdo_get_database_object_from_object(obj TSRMLS_CC);
  if (NULL == dbh) {
    nrl_verbosedebug(NRL_SQL, "%s: unable to get pdo_dbh_t", __func__);
    return NULL;
  }

  return nr_php_pdo_get_driver_internal(dbh);
}

nr_datastore_t nr_php_pdo_get_datastore_for_driver(const char* driver_name) {
  size_t i;

  if (NULL == driver_name) {
    return NR_DATASTORE_PDO;
  }

  for (i = 0; nr_php_pdo_datastore_mappings[i].driver_name; i++) {
    const nr_php_pdo_datastore_mapping_t* mapping
        = &nr_php_pdo_datastore_mappings[i];

    if (0 == nr_strcmp(mapping->driver_name, driver_name)) {
      return mapping->datastore;
    }
  }

  return NR_DATASTORE_PDO;
}

nr_datastore_t nr_php_pdo_get_datastore_internal(pdo_dbh_t* dbh) {
  const char* driver_name = nr_php_pdo_get_driver_internal(dbh);

  return nr_php_pdo_get_datastore_for_driver(driver_name);
}

nr_datastore_t nr_php_pdo_get_datastore(zval* obj TSRMLS_DC) {
  pdo_dbh_t* dbh = nr_php_pdo_get_database_object_from_object(obj TSRMLS_CC);

  return nr_php_pdo_get_datastore_internal(dbh);
}

char* nr_php_pdo_datastore_make_key(pdo_dbh_t* dbh) {
  if ((NULL == dbh) || (NULL == dbh->data_source)
      || (dbh->data_source_len <= 0)) {
    return NULL;
  }

  /*
   * Because we don't always have access to the PDO object when creating an SQL
   * node for a PDO query, we'll index the metadata based on the DSN instead
   * (which we _can_ always access, since we have either a PDO object or a
   * PDOStatement object, and both contain that pointer).
   */

  return nr_formatf("type=pdo driver=%s dsn=%.*s",
                    NRSAFESTR(nr_php_pdo_get_driver_internal(dbh)),
                    NRSAFELEN(dbh->data_source_len), dbh->data_source);
}

/*
 * Handler functions to create datastore instance metadata for a particular PDO
 * driver are defined below.
 */
static const struct {
  nr_datastore_t datastore;
  nr_datastore_instance_t* (*handler)(pdo_dbh_t* dbh TSRMLS_DC);
} instance_handlers[] = {
    {NR_DATASTORE_MYSQL, nr_php_pdo_mysql_create_datastore_instance},
    {NR_DATASTORE_POSTGRES, nr_php_pdo_pgsql_create_datastore_instance},
};
static const size_t num_instance_handlers
    = sizeof(instance_handlers) / sizeof(instance_handlers[0]);

nr_datastore_instance_t* nr_php_pdo_get_datastore_instance(
    zval* obj TSRMLS_DC) {
  nr_datastore_t datastore;
  pdo_dbh_t* dbh = nr_php_pdo_get_database_object_from_object(obj TSRMLS_CC);
  nr_datastore_instance_t* instance;
  size_t i;
  char* key;

  key = nr_php_pdo_datastore_make_key(dbh);
  if (NULL == key) {
    nrl_verbosedebug(NRL_SQL, "%s: cannot make key for PDO object", __func__);
    return NULL;
  }

  /*
   * If the instance information is already in the cache, then let's just return
   * that.
   */
  instance = nr_php_datastore_instance_retrieve(key TSRMLS_CC);
  if (instance) {
    goto end;
  }

  /*
   * If the instance information is not in the cache, create instance
   */
  datastore = nr_php_pdo_get_datastore_internal(dbh);
  for (i = 0; i < num_instance_handlers; i++) {
    if (instance_handlers[i].datastore == datastore) {
      instance = (instance_handlers[i].handler)(dbh TSRMLS_CC);
      if (instance) {
        nr_php_datastore_instance_save(key, instance TSRMLS_CC);
      } else {
        nrl_verbosedebug(NRL_SQL,
                         "%s: unable to create datastore instance metadata "
                         "for supported datastore %d",
                         __func__, (int)datastore);
      }
      goto end;
    }
  }

end:
  nr_free(key);
  return instance;
}

void nr_php_pdo_end_segment_sql(nr_segment_t* segment,
                                const char* sqlstr,
                                size_t sqlstrlen,
                                zval* stmt_obj,
                                zval* parameters,
                                bool try_explain TSRMLS_DC) {
  nr_datastore_t datastore;
  pdo_dbh_t* dbh
      = nr_php_pdo_get_database_object_from_object(stmt_obj TSRMLS_CC);
  nr_datastore_instance_t* instance;
  nr_explain_plan_t* plan = NULL;

  if (try_explain && (NULL != segment)) {
    /*
     * Do not count explain plan time in the datastore segment.
     */
    if (0 == segment->stop_time) {
      segment->stop_time = nr_txn_now_rel(segment->txn);
    }

    plan = nr_php_explain_pdo_statement(segment->txn, stmt_obj, parameters,
                                        segment->start_time,
                                        segment->stop_time TSRMLS_CC);
  }

  datastore = nr_php_pdo_get_datastore_internal(dbh);
  instance = nr_php_pdo_get_datastore_instance(stmt_obj TSRMLS_CC);
  nr_php_txn_end_segment_sql(&segment, sqlstr, sqlstrlen, plan, datastore,
                             instance TSRMLS_CC);

  nr_explain_plan_destroy(&plan);
}

static zval* nr_php_pdo_options_get(zval* dbh TSRMLS_DC) {
  if (NULL == NRTXNGLOBAL(pdo_link_options)) {
    return NULL;
  }

  return (zval*)nr_hashmap_index_get(NRTXNGLOBAL(pdo_link_options),
                                     Z_OBJ_HANDLE_P(dbh));
}

zval* nr_php_pdo_duplicate(zval* dbh TSRMLS_DC) {
  zend_uint argc = 3;
  zval* argv[4] = {NULL, NULL, NULL, NULL};
  const char* driver = NULL;
  char* dsn = NULL;
  int dsn_len;
  zval* dup = NULL;
  zval* exception = NULL;
  zval* options;
  pdo_dbh_t* pdo_dbh;
  zval* retval = NULL;
  zend_class_entry* pdo_ce = NULL;

  pdo_dbh = nr_php_pdo_get_database_object(dbh TSRMLS_CC);
  if (NULL == pdo_dbh) {
    return NULL;
  }

  /*
   * We perform a lookup instead of using Z_OBJCE_P (dbh) to ensure we
   * instantiate an instance of PDO rather than a subclass, which might
   * have a different constructor.
   */
  pdo_ce = nr_php_find_class("pdo" TSRMLS_CC);
  if (NULL == pdo_ce) {
    return NULL;
  }

  /*
   * We'll always provide the first three arguments to PDO::__construct(), as
   * it can handle NULLs if the username and/or password weren't provided.
   */

  /*
   * The DSN in the pdo_dbh_t struct doesn't include the driver name, so let's
   * get that and build up a new DSN.
   */
  driver = nr_php_pdo_get_driver(dbh TSRMLS_CC);
  dsn_len = asprintf(&dsn, "%s:%.*s", driver,
                     NRSAFELEN(pdo_dbh->data_source_len), pdo_dbh->data_source);
  argv[0] = nr_php_zval_alloc();
  nr_php_zval_str_len(argv[0], dsn, dsn_len);
  nr_free(dsn);

  argv[1] = nr_php_zval_alloc();
  if (pdo_dbh->username) {
    nr_php_zval_str(argv[1], pdo_dbh->username);
  } else {
    ZVAL_NULL(argv[1]);
  }

  argv[2] = nr_php_zval_alloc();
  if (pdo_dbh->password) {
    nr_php_zval_str(argv[2], pdo_dbh->password);
  } else {
    ZVAL_NULL(argv[2]);
  }

  /*
   * We'll only provide options if there actually are some, since we don't own
   * the options zval, and if we provided a NULL we'd have to free it later.
   */
  options = nr_php_pdo_options_get(dbh TSRMLS_CC);
  if (options) {
    argv[3] = nr_php_pdo_disable_persistence(options TSRMLS_CC);
    argc++;
  }

  /*
   * Create the object and construct it.
   */
  dup = nr_php_zval_alloc();
  object_init_ex(dup, pdo_ce);
  retval = nr_php_call_user_func_catch(dup, "__construct", argc, argv,
                                       &exception TSRMLS_CC);

  if ((NULL == retval) || (NULL != exception)) {
    nr_php_zval_free(&dup);
    nr_php_zval_free(&exception);
  }

  nr_php_zval_free(&argv[0]);
  nr_php_zval_free(&argv[1]);
  nr_php_zval_free(&argv[2]);
  nr_php_zval_free(&argv[3]);
  nr_php_zval_free(&retval);

  return dup;
}

static void free_options(zval* options) {
  nr_php_zval_free(&options);
}

void nr_php_pdo_options_save(zval* dbh, zval* options TSRMLS_DC) {
  zval* copy = NULL;

  if (0 == nr_php_is_zval_valid_array(options)) {
    return;
  }

  if (0 == nr_php_object_instanceof_class(dbh, "PDO" TSRMLS_CC)) {
    return;
  }

  /*
   * Lazily create the link options hashmap if it isn't already created.
   */
  if (NULL == NRTXNGLOBAL(pdo_link_options)) {
    NRTXNGLOBAL(pdo_link_options)
        = nr_hashmap_create((nr_hashmap_dtor_func_t)free_options);
  }

  copy = nr_php_zval_alloc();
  ZVAL_DUP(copy, options);
  nr_hashmap_index_update(NRTXNGLOBAL(pdo_link_options), Z_OBJ_HANDLE_P(dbh),
                          copy);
}

/*
 * Weakly declare PDO's php_pdo_parse_data_source() function so we can use it.
 * Its signature doesn't change between PHP 5 and 7, with the
 * exception of the way zend_ulong itself changed.
 */
extern int __attribute__((weak))
php_pdo_parse_data_source(const char* data_source,
                          zend_ulong data_source_len,
                          struct pdo_data_src_parser* parsed,
                          int nparams);

nr_status_t nr_php_pdo_parse_data_source(const char* data_source,
                                         uint64_t data_source_len,
                                         struct pdo_data_src_parser* parsed,
                                         size_t nparams) {
  if (!php_pdo_parse_data_source) {
    nrl_verbosedebug(NRL_SQL, "%s: php_pdo_parse_data_source() unavailable",
                     __func__);
    return NR_FAILURE;
  }

  if (nparams > INT_MAX) {
    nrl_verbosedebug(NRL_SQL, "%s: invalid number of parameters provided: %zu",
                     __func__, nparams);
    return NR_FAILURE;
  }

  php_pdo_parse_data_source(data_source, (zend_ulong)data_source_len, parsed,
                            (int)nparams);
  return NR_SUCCESS;
}

void nr_php_pdo_free_data_sources(struct pdo_data_src_parser* parsed,
                                  size_t nparams) {
  size_t i;

  for (i = 0; i < nparams; i++) {
    if (parsed[i].freeme) {
      efree(parsed[i].optval);
    }
  }
}

zval* nr_php_pdo_disable_persistence(const zval* options TSRMLS_DC) {
  const zend_class_entry* pdo_ce;
  zend_ulong num_key = 0;
  zval* persistent;
  zval* result;
  nr_php_string_hash_key_t* string_key;
  zval* value;

  if (!nr_php_is_zval_valid_array(options)) {
    if (options) {
      nrl_verbosedebug(
          NRL_SQL, "unexpected type for the options array: expected %d; got %d",
          IS_ARRAY, (int)Z_TYPE_P(options));
    } else {
      nrl_verbosedebug(NRL_SQL, "unexpected NULL options array");
    }

    return NULL;
  }

  /*
   * We need to get the actual value of the PDO::ATTR_PERSISTENT class
   * constant. Firstly, we need to find the class entry itself.
   */
  pdo_ce = nr_php_find_class("pdo" TSRMLS_CC);
  if (NULL == pdo_ce) {
    // Log, since we shouldn't get here if PDO is unavailable.
    nrl_verbosedebug(NRL_SQL, "cannot get class entry for PDO");
    return NULL;
  }

  /*
   * Secondly, we need to get the class constant zval, which should be an
   * integer. In practice, the value is 12 in every PHP version we support, but
   * this is not guaranteed by the API.
   */
  persistent = nr_php_get_class_constant(pdo_ce, "ATTR_PERSISTENT");
  if (!nr_php_is_zval_valid_integer(persistent)) {
    if (persistent) {
      nrl_verbosedebug(
          NRL_SQL,
          "unexpected type for PDO::ATTR_PERSISTENT: expected %d; got %d",
          IS_LONG, (int)Z_TYPE_P(persistent));

      nr_php_zval_free(&persistent);
    } else {
      nrl_verbosedebug(NRL_SQL, "unexpected NULL PDO::ATTR_PERSISTENT");
    }

    return NULL;
  }

  /*
   * Now we can allocate an empty array and start copying values in from the
   * input options.
   */
  result = nr_php_zval_alloc();
  array_init(result);

  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(options), num_key, string_key, value) {
    /*
     * On a high level, what we want to do is copy every value as is unless the
     * key is PDO::ATTR_PERSISTENT, in which case we'll insert a `false` value.
     *
     * We'll use nr_php_add_assoc_zval() or nr_php_add_index_zval() to do the
     * copying, since those functions duplicate the zval before inserting them
     * into the new array.
     */

    if (string_key) {
      /*
       * We know that PDO::ATTR_PERSISTENT is an integer, so if there's a
       * string key, we can just copy the value and move on.
       */
      nr_php_add_assoc_zval(result, ZEND_STRING_VALUE(string_key), value);
    } else if (num_key == (zend_ulong)Z_LVAL_P(persistent)) {
      /*
       * In this interesting case, the key is an integer, and it matches
       * PDO::ATTR_PERSISTENT. Regardless of the input value, we're going to
       * force the result array to have `false` here to ensure that persistent
       * connections are disabled.
       */
      zval* zv_false = nr_php_zval_alloc();

      nr_php_zval_bool(zv_false, 0);
      nr_php_add_index_zval(result, num_key, zv_false);
      nr_php_zval_free(&zv_false);
    } else {
      // Any other integer key can also be copied as is.
      nr_php_add_index_zval(result, num_key, value);
    }
  }
  ZEND_HASH_FOREACH_END();

  nr_php_zval_free(&persistent);
  return result;
}
