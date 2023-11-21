/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting Doctrine ORM or DBAL 2.
 *
 * Implemented according to the newrelic-internal SQL Input Query Spec.
 */
#include "php_agent.h"
#include "php_wrapper.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_sql.h"
#include "php_call.h"
#include "lib_doctrine2.h"

/*
 * This answers the somewhat complicated question of whether we should
 * instrument DQL, which is dependent on the input query setting as well as SQL
 * settings.
 */
static int nr_doctrine2_dql_enabled(TSRMLS_D) {
  return ((NR_SQL_NONE != nr_txn_sql_recording_level(NRPRG(txn)))
          && NRINI(tt_inputquery));
}

NR_PHP_WRAPPER(nr_doctrine2_cache_dql) {
  (void)wraprec;

  if (nr_doctrine2_dql_enabled(TSRMLS_C)) {
    zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    if (0
        != nr_php_object_instanceof_class(this_var,
                                          "Doctrine\\ORM\\Query" TSRMLS_CC)) {
      zval* dql = nr_php_call(this_var, "getDQL");

      if (nr_php_is_zval_valid_string(dql)) {
        nr_free(NRPRG(doctrine_dql));
        NRPRG(doctrine_dql) = nr_strndup(Z_STRVAL_P(dql), Z_STRLEN_P(dql));
      }

      nr_php_zval_free(&dql);
    }
    nr_php_scope_release(&this_var);
  }

  NR_PHP_WRAPPER_CALL;

  nr_free(NRPRG(doctrine_dql));
}
NR_PHP_WRAPPER_END

nr_slowsqls_labelled_query_t* nr_doctrine2_lookup_input_query(TSRMLS_D) {
  nr_slowsqls_labelled_query_t* query = NULL;
  const char* dql = NRPRG(doctrine_dql);

  if (NULL == dql) {
    return NULL;
  }

  if (0 == nr_doctrine2_dql_enabled(TSRMLS_C)) {
    return NULL;
  }

  query = nr_malloc(sizeof(nr_slowsqls_labelled_query_t));

  query->query = dql;
  query->name = "Doctrine DQL";

  return query;
}

void nr_doctrine2_enable(TSRMLS_D) {
  nr_php_wrap_user_function(NR_PSTR("Doctrine\\ORM\\Query::_doExecute"),
                            nr_doctrine2_cache_dql TSRMLS_CC);
}
