/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "fw_symfony_common.h"
#include "util_logging.h"
#include "util_memory.h"

/*
 * Helper to handle the basics of naming a transaction based on the
 * string value of a zval.
 */
int nr_symfony_name_the_wt_from_zval(const zval* name TSRMLS_DC,
                                     const char* symfony_version) {
  if (nrlikely(nr_php_is_zval_non_empty_string(name))) {
    char* path = nr_strndup(Z_STRVAL_P(name), Z_STRLEN_P(name));

    nr_txn_set_path(
        symfony_version, NRPRG(txn), path, NR_PATH_TYPE_ACTION,
        NR_OK_TO_OVERWRITE); /* Watch out: this name is OK to overwrite */

    nr_free(path);
    return NR_SUCCESS;
  }

  return NR_FAILURE;
}

/*
 * Call the get method on the given object and return a string zval if a valid
 * string was returned. The result must be freed.
 */
zval* nr_symfony_object_get_string(zval* obj, const char* param TSRMLS_DC) {
  zval* rval = NULL;
  zval* param_zv = nr_php_zval_alloc();

  nr_php_zval_str(param_zv, param);
  rval = nr_php_call(obj, "get", param_zv);
  nr_php_zval_free(&param_zv);

  if (NULL == rval) {
    nrl_verbosedebug(NRL_TXN, "Error calling get('%s')", param);
  } else if (!nr_php_is_zval_non_empty_string(rval)) {
    nr_php_zval_free(&rval);
  }

  return rval;
}
