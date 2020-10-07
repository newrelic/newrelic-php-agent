/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "fw_codeigniter.h"
#include "fw_support.h"
#include "fw_hooks.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

zend_op_array* nr_codeigniter_get_topmost_user_op_array(TSRMLS_D) {
#ifdef PHP7
  zend_execute_data* ed = NULL;

  for (ed = EG(current_execute_data); ed; ed = ed->prev_execute_data) {
    if (ed->func
        && (ZEND_USER_FUNCTION == ed->func->common.type
            || ZEND_EVAL_CODE == ed->func->common.type)) {
      return &ed->func->op_array;
    }
  }

  return NULL;
#else
  return EG(current_execute_data)->op_array;
#endif /* PHP7 */
}

/*
 * Determine the WT name from the CodeIgniter dispatcher.
 * Usage: called from a specific internal function wrapper
 */
static void nr_codeigniter_name_the_wt(zend_function* func,
                                       const zend_function* caller NRUNUSED
                                           TSRMLS_DC) {
  zend_op_array* op_array = NULL;

  if ((NR_FW_CODEIGNITER != NRPRG(current_framework) || (NULL == func)
       || (NULL == func->common.scope))) {
    return;
  }

  op_array = nr_codeigniter_get_topmost_user_op_array(TSRMLS_C);
  if (NULL == op_array) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "CodeIgniter: unable to get the topmost user function");
    return;
  }

  /*
   * We're looking for a particular active call stack:
   *   1. (php file) CodeIgniter.php
   *   ..calls..
   *   2. (internal function) call_user_func_array( <action>, ... )
   */
  if (nr_strcaseidx(nr_php_op_array_file_name(op_array), "codeigniter.php")
      >= 0) {
    char* action = NULL;
    zend_class_entry* ce = func->common.scope;

    /*
     * The codeigniter name is the class and method being passed as an
     * array as first parameter to call_user_func_array.
     */

    action = nr_formatf("%.*s/%.*s", (int)nr_php_class_entry_name_length(ce),
                        nr_php_class_entry_name(ce),
                        (int)nr_php_function_name_length(func),
                        nr_php_function_name(func));

    nr_txn_set_path("CodeIgniter", NRPRG(txn), action, NR_PATH_TYPE_ACTION,
                    NR_NOT_OK_TO_OVERWRITE);

    nr_free(action);
  }
}

/*
 * Enable CodeIgniter instrumentation
 */
void nr_codeigniter_enable(TSRMLS_D) {
  nr_php_add_call_user_func_array_pre_callback(
      nr_codeigniter_name_the_wt TSRMLS_CC);
}
