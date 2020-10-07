/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static void nr_joomla_txn_set_path(const char* class_name,
                                   const nr_string_len_t class_name_length,
                                   const zval* action_name TSRMLS_DC) {
  char* name
      = (char*)nr_alloca(class_name_length + 1 + Z_STRLEN_P(action_name) + 1);

  nr_strxcpy(name, class_name, class_name_length);
  name[class_name_length] = '/';
  nr_strxcpy(name + class_name_length + 1, Z_STRVAL_P(action_name),
             Z_STRLEN_P(action_name));

  nr_txn_set_path("Joomla", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
}

/* First argument is a string which is the action name */
NR_PHP_WRAPPER(nr_joomla_name_the_wt) {
  zval* arg1 = NULL;
  zval* this_var = NULL;
  zend_class_entry* ce = NULL;
  const char* class_name = NULL;
  nr_string_len_t class_name_length;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_JOOMLA);

  /* Class name of this is the controller name. */
  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (!nr_php_is_zval_valid_object(this_var)) {
    goto leave;
  }

  ce = Z_OBJCE_P(this_var);
  class_name = nr_php_class_entry_name(ce);
  class_name_length = nr_php_class_entry_name_length(ce);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string(arg1)) {
    nr_joomla_txn_set_path(class_name, class_name_length, arg1 TSRMLS_CC);
  } else if (arg1) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Joomla: type=%d", Z_TYPE_P(arg1));
  }

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&arg1);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/* First argument may optionally be string which is the action name */
NR_PHP_WRAPPER(nr_joomla3_name_the_wt) {
  zval* arg1 = NULL;
  zval* this_var = NULL;
  zend_class_entry* ce = NULL;
  const char* class_name = NULL;
  nr_string_len_t class_name_length;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_JOOMLA);

  /* Class name of this is the controller name. */
  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (!nr_php_is_zval_valid_object(this_var)) {
    goto leave;
  }

  ce = Z_OBJCE_P(this_var);
  class_name = nr_php_class_entry_name(ce);
  class_name_length = nr_php_class_entry_name_length(ce);

  /*
   * Like prior Joomla!: attempt first to gather the action from the first
   * parameter.
   */
  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string(arg1)) {
    nr_joomla_txn_set_path(class_name, class_name_length, arg1 TSRMLS_CC);
  } else {
    /*
     * If there was no arg1 specified, then we are going to invoke the default
     * task. Get it by reading the value of taskMap['__default'] from this.
     */
    zval* task_map_zval = NULL;
    zval* default_zval = NULL;

    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Joomla: no parameter 1 passed to "
                     "JControllerLegacy::execute(); using taskMap['__default'] "
                     "as the action name");

    task_map_zval
        = nr_php_get_zval_object_property(this_var, "taskMap" TSRMLS_CC);
    if (NULL == task_map_zval) {
      nrl_verbosedebug(NRL_FRAMEWORK, "Joomla: no taskMap found in component");
      goto leave;
    }

    if (IS_ARRAY != Z_TYPE_P(task_map_zval)) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "Joomla: component taskMap is not an array");
      goto leave;
    }

    default_zval
        = nr_php_get_zval_object_property(task_map_zval, "__default" TSRMLS_CC);
    if (nr_php_is_zval_non_empty_string(default_zval)) {
      nr_joomla_txn_set_path(class_name, class_name_length,
                             default_zval TSRMLS_CC);
    } else {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "Joomla: no taskMap['__default'] in component");
    }
  }

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&arg1);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Enable the Joomla instrumentation.
 */
void nr_joomla_enable(TSRMLS_D) {
  /* Note the intentional spelling difference! */
  nr_php_wrap_user_function(NR_PSTR("JController::authorize"),
                            nr_joomla_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("JController::authorise"),
                            nr_joomla_name_the_wt TSRMLS_CC);

  /*
   * joomla3 fundamentally changed the execution trace;
   * JController::authorise/ize is no longer in the trace in v3.2 so we need to
   * find something else to hook into.
   *
   * JControllerLegacy::execute appears to be a viable candidate.
   *
   * Note that in v2.5 *both* JController::authorise/ize and
   * JControllerLegacy::execute exist in the trace. We are using a "first one
   * wins" naming policy here (i.e., NR_NOT_OK_TO_OVERWRITE).  Tests so far
   * indicate that they would have produced the same naming answer anyway, so
   * this appears to be an inconsequential choice and seems the most
   * conservative thing to do at this point.
   */
  nr_php_wrap_user_function(NR_PSTR("JControllerLegacy::execute"),
                            nr_joomla3_name_the_wt TSRMLS_CC);
}
