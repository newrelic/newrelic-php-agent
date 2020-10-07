/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_datastore.h"
#include "lib_doctrine2.h"
#include "nr_segment_datastore.h"
#include "util_logging.h"

/*
 * Magento 2 temporary table names need to be squashed to avoid MGIs.
 * Example: search_tmp_5771897a542b48_79048580 -> search_tmp_*
 */
static void nr_php_modify_sql_table_name_magento2(char* tablename) {
  const char* prefix = "search_tmp_";
  int prefix_len = nr_strlen(prefix);

  if (nr_strlen(tablename) <= prefix_len) {
    return;
  }

  if (0 == nr_strncmp(prefix, tablename, prefix_len)) {
    tablename[prefix_len] = '*';
    tablename[prefix_len + 1] = '\0';
  }
}

/*
 *In order to avoid the metrics explosion that occurs when Wordpress 
 *duplicates all of the tables when a new blog is created, if the
 * current framework is Wordpress, we make the following substitution:
 * wp_\([0-9]*\)_\(.*\) -> wp_*_\2.
 *
 * http://codex.wordpress.org/Database_Description#Site_Specific_Tables
 */
static void nr_php_modify_sql_table_name_wordpress(char* tablename) {
  if (NULL == tablename) {
    return;
  }
  if (0 == tablename[0]) {
    return;
  }

  if (('w' == tablename[0]) && ('p' == tablename[1]) && ('_' == tablename[2])) {
    char* dp = tablename + 3;

    while ((*dp) && (nr_isdigit(*dp))) {
      dp++;
    }

    if ((dp != (tablename + 3)) && ('_' == *dp)) {
      int i;

      tablename[3] = '*';

      /* Avoid strcpy and memcpy since there is overlap. */
      for (i = 0; dp[i]; i++) {
        tablename[4 + i] = dp[i];
      }
      tablename[4 + i] = '\0';
    }
  }
}

static nr_modify_table_name_fn_t nr_php_modify_table_name_fn(TSRMLS_D) {
  nr_modify_table_name_fn_t modify_table_name_fn = NULL;

  if (NR_FW_WORDPRESS == NRPRG(current_framework)) {
    modify_table_name_fn = &nr_php_modify_sql_table_name_wordpress;
  }

  if (NR_FW_MAGENTO2 == NRPRG(current_framework)) {
    modify_table_name_fn = &nr_php_modify_sql_table_name_magento2;
  }

  return modify_table_name_fn;
}

void nr_php_txn_end_segment_sql(nr_segment_t** segment_ptr,
                                const char* sql,
                                int sqllen,
                                const nr_explain_plan_t* plan,
                                nr_datastore_t datastore,
                                nr_datastore_instance_t* instance TSRMLS_DC) {
  nr_slowsqls_labelled_query_t* input_query = NULL;
  char* plan_json = NULL;
  char* terminated_sql;
  nr_modify_table_name_fn_t modify_table_name_fn = NULL;
  nr_segment_t* segment = NULL;

  if ((NULL == segment_ptr) || (NULL == *segment_ptr) || (NULL == sql)
      || ('\0' == *sql) || (sqllen <= 0)) {
    goto error;
  }

  segment = *segment_ptr;

  /*
   * Bail early if this is a nested explain plan query.
   */
  if (NRTXNGLOBAL(generating_explain_plan)) {
    goto error;
  }

  /*
   * Export the explain plan as JSON, assuming if we got one.
   */
  if (plan) {
    plan_json = nr_explain_plan_to_json(plan);
  }

  input_query = nr_doctrine2_lookup_input_query(TSRMLS_C);
  terminated_sql = nr_strndup(sql, sqllen);
  modify_table_name_fn = nr_php_modify_table_name_fn(TSRMLS_C);

  {
    nr_segment_datastore_params_t params = {
      .instance     = instance,
      .datastore    = {
        .type = datastore,
      },
      .sql          = {
        .sql = terminated_sql,
        .plan_json    = plan_json,
        .input_query  = input_query,
      },
      .callbacks    = {
        .backtrace = &nr_php_backtrace_callback,
        .modify_table_name = modify_table_name_fn,
      },
    };

    nr_segment_datastore_end(&segment, &params);
  }

  nr_free(terminated_sql);
  nr_free(plan_json);
  nr_free(input_query);

  return;

error:
  if (NULL != segment) {
    nr_segment_discard(&segment);
  }
}

char* nr_php_datastore_make_key(const zval* conn, const char* extension) {
  char* key = NULL;

  if (nr_php_is_zval_valid_resource(conn)) {
    key = nr_formatf("type=resource id=%ld", nr_php_zval_resource_id(conn));
  } else if (nr_php_is_zval_valid_object(conn)) {
    key = nr_formatf("type=object id=%lu", (unsigned long)Z_OBJ_HANDLE_P(conn));
  } else if (NULL == conn) {
    key = nr_formatf("type=%s id=0", NRSAFESTR(extension));
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s conn is unexpected type %d; expected resource, "
                     "object, or conn to be NULL",
                     NRSAFESTR(extension), Z_TYPE_P(conn));
  }

  return key;
}

int nr_php_datastore_has_conn(const char* key TSRMLS_DC) {
  if (NULL == key) {
    return 0;
  }

  return nr_hashmap_has(NRPRG(datastore_connections), key, nr_strlen(key));
}

void nr_php_datastore_instance_save(const char* key,
                                    nr_datastore_instance_t* instance
                                        TSRMLS_DC) {
  if ((NULL == key) || (NULL == instance)) {
    return;
  }

  nr_hashmap_update(NRPRG(datastore_connections), key, nr_strlen(key),
                    instance);
}

nr_datastore_instance_t* nr_php_datastore_instance_retrieve(
    const char* key TSRMLS_DC) {
  if (NULL == key) {
    return NULL;
  }

  return (nr_datastore_instance_t*)nr_hashmap_get(NRPRG(datastore_connections),
                                                  key, nr_strlen(key));
}

void nr_php_datastore_instance_remove(const char* key TSRMLS_DC) {
  if (NULL == key) {
    return;
  }

  nr_hashmap_delete(NRPRG(datastore_connections), key, nr_strlen(key));
}
