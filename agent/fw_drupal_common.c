/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_drupal_common.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_strings.h"

int nr_drupal_do_view_execute(const char* name,
                              int name_len,
                              nr_segment_t* segment,
                              NR_EXECUTE_PROTO TSRMLS_DC) {
  nr_drupal_create_metric(segment, NR_PSTR(NR_DRUPAL_VIEW_PREFIX), name,
                          name_len);

  return nr_zend_call_orig_execute(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
}

void nr_drupal_create_metric(nr_segment_t* segment,
                             const char* prefix,
                             int prefix_len,
                             const char* suffix,
                             int suffix_len) {
  char* name = NULL;
  char* nm = NULL;

  name = (char*)nr_alloca(prefix_len + suffix_len + 2);
  nm = nr_strxcpy(name, prefix, prefix_len);
  nr_strxcpy(nm, suffix, suffix_len);

  nr_segment_add_metric(segment, name, false);
}

int nr_drupal_is_framework(nrframework_t fw) {
  return ((NR_FW_DRUPAL == fw) || (NR_FW_DRUPAL8 == fw));
}

/*
 * Purpose : Wrap a module hook function to generate module and hook metrics.
 */
NR_PHP_WRAPPER(nr_drupal_wrap_module_hook) {
  if (!nr_drupal_is_framework(NRPRG(current_framework))) {
    NR_PHP_WRAPPER_LEAVE;
  }

  NR_PHP_WRAPPER_CALL;

  /*
   * We can't infer the module and hook names from the function name, since a
   * function such as a_b_c is ambiguous (is the module a or a_b?). Instead,
   * we'll see if they're defined in the wraprec.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_2_X_API_NO
  wraprec = nr_php_get_wraprec(execute_data->func);
#endif
  if ((NULL != wraprec->drupal_hook) && (NULL != wraprec->drupal_module)) {
    nr_drupal_create_metric(auto_segment, NR_PSTR(NR_DRUPAL_MODULE_PREFIX),
                            wraprec->drupal_module, wraprec->drupal_module_len);
    nr_drupal_create_metric(auto_segment, NR_PSTR(NR_DRUPAL_HOOK_PREFIX),
                            wraprec->drupal_hook, wraprec->drupal_hook_len);
  }
}
NR_PHP_WRAPPER_END

nruserfn_t* nr_php_wrap_user_function_drupal(const char* name,
                                             int namelen,
                                             const char* module,
                                             nr_string_len_t module_len,
                                             const char* hook,
                                             nr_string_len_t hook_len
                                                 TSRMLS_DC) {
  nruserfn_t* wraprec;

  wraprec = nr_php_wrap_user_function(name, namelen,
                                      nr_drupal_wrap_module_hook TSRMLS_CC);
  if (wraprec) {
    /*
     * As wraprecs can be reused, we need to free any previous hook or module
     * to avoid memory leaks.
     */
    nr_free(wraprec->drupal_hook);
    nr_free(wraprec->drupal_module);

    wraprec->drupal_hook = nr_strndup(hook, hook_len);
    wraprec->drupal_hook_len = hook_len;
    wraprec->drupal_module = nr_strndup(module, module_len);
    wraprec->drupal_module_len = module_len;
  }

  return wraprec;
}

void nr_drupal_hook_instrument(const char* module,
                               size_t module_len,
                               const char* hook,
                               size_t hook_len TSRMLS_DC) {
  size_t function_name_len = 0;
  char* function_name = NULL;

  /*
   * Construct the name of the function we need to instrument from the module
   * and hook names.
   */
  function_name_len = module_len + hook_len + 2;
  function_name = nr_alloca(function_name_len);

  nr_strxcpy(function_name, module, module_len);
  nr_strcat(function_name, "_");
  nr_strncat(function_name, hook, hook_len);

  /*
   * Actually instrument the function.
   */
  nr_php_wrap_user_function_drupal(function_name, function_name_len - 1, module,
                                   module_len, hook, hook_len TSRMLS_CC);
}

nr_status_t module_invoke_all_parse_module_and_hook_from_strings(
    char** module_ptr,
    size_t* module_len_ptr,
    const char* hook,
    size_t hook_len,
    const char* module_hook,
    size_t module_hook_len) {
  size_t module_len = 0;
  char* module = NULL;

  if ((0 == module_hook) || (module_hook_len <= 0)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get function name", __func__);
    return NR_FAILURE;
  }

  if (hook_len >= module_hook_len) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: hook length (%zu) is greater than the full module "
                     "hook function length (%zu); "
                     "hook='%.*s'; module_hook='%.*s'",
                     __func__, hook_len, module_hook_len, NRSAFELEN(hook_len),
                     NRSAFESTR(hook), NRSAFELEN(module_hook_len),
                     NRSAFESTR(module_hook));
    return NR_FAILURE;
  }

  module_len = (size_t)nr_strnidx(module_hook, hook, module_hook_len)
               - 1; /* Subtract 1 for underscore separator */

  if (module_len == 0) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: cannot find hook in module hook; "
                     "hook='%.*s'; module_hook='%.*s'",
                     __func__, NRSAFELEN(hook_len), NRSAFESTR(hook),
                     NRSAFELEN(module_hook_len), NRSAFESTR(module_hook));
    return NR_FAILURE;
  }

  /*
   * a -1 module length means the hook name matches the module name
   * modulename: atlas_statistics
   * hookname:   atlas_stat
   * hookname:   atlas_statistics
   * etc. If that's the case we set the module_len to be something
   * that will result in the correct module name being set
   */
  if (-1 == (int)module_len) {
    module_len = module_hook_len - hook_len
                 - 1; /* Subtract 1 for underscore separator */
  }

  if (-1 >= (int)module_len) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: module len is %d; ; "
                     "hook='%.*s'; module_hook='%.*s'",
                     __func__, (int)module_len, NRSAFELEN(hook_len), NRSAFESTR(hook),
                     NRSAFELEN(module_hook_len), NRSAFESTR(module_hook));

    return NR_FAILURE;
  }

  module = nr_strndup(module_hook, module_len);

  *module_ptr = module;
  *module_len_ptr = (size_t)module_len;

  return NR_SUCCESS;
}

nr_status_t module_invoke_all_parse_module_and_hook(char** module_ptr,
                                                    size_t* module_len_ptr,
                                                    const char* hook,
                                                    size_t hook_len,
                                                    const zend_function* func) {
  const char* module_hook = NULL;
  size_t module_hook_len = 0;

  *module_ptr = NULL;
  *module_len_ptr = 0;

  if (NULL == func) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: func is NULL", __func__);
    return NR_FAILURE;
  }

  module_hook = nr_php_function_name(func);
  module_hook_len = (size_t)nr_php_function_name_length(func);

  return module_invoke_all_parse_module_and_hook_from_strings(
      module_ptr, module_len_ptr, hook, hook_len, module_hook, module_hook_len);
}

void nr_drupal_headers_add(zval* arg, bool is_drupal_7 TSRMLS_DC) {
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;

  zval* headers = NULL;
  zval* newrelic_headers = NULL;

  if (NULL == arg) {
    return;
  }

  /*
   * For Drupal 6, a 'NULL' argument is replaced with an empty array.
   * For Drupal 7 that is not done and thus causes a TypeError.
   */
  if (!is_drupal_7 && nr_php_is_zval_null(arg)) {
    array_init(arg);
  }

  /*
   * (Invalid) arguments that are not an array are left untouched, thus
   * leaving it to the wrapped function to raise a TypeError.
   */
  if (!nr_php_is_zval_valid_array(arg)) {
    return;
  }

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  SEPARATE_ARRAY(arg);
#endif /* PHP >= 7.3 */

  /*
   * The following code block ensures that 'headers' points to the PHP
   * array containing request header key/value pairs. 'headers' will
   * point to the plain second argument ($headers) for Drupal 6 and to
   * the value of the "headers" key of the second argument
   * ($options["headers"]) for Drupal 7.
   */
  if (is_drupal_7) {
    headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");
    if (NULL == headers) {
      headers = nr_php_zval_alloc();
      array_init(headers);

      nr_php_add_assoc_zval(arg, "headers", headers);

      nr_php_zval_free(&headers);

      headers = nr_php_zend_hash_find(Z_ARRVAL_P(arg), "headers");
    } else if (nr_php_is_zval_valid_array(headers)) {
#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
      SEPARATE_ARRAY(headers);
#endif /* PHP >= 7.3 */
    } else {
      return;
    }
  } else {
    headers = arg;
  }

  /*
   * New Relic headers are created and added to the 'headers' array.
   */
  newrelic_headers = nr_php_call(NULL, "newrelic_get_request_metadata");

  if (newrelic_headers && headers) {
    ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(newrelic_headers), key_num, key_str,
                              val) {
      (void)key_num;

      if (key_str) {
        nr_php_add_assoc_zval(headers, ZEND_STRING_VALUE(key_str), val);
      }
    }
    ZEND_HASH_FOREACH_END();
  }

  nr_php_zval_free(&newrelic_headers);
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
void nr_drupal_invoke_all_hook_stacks_push(zval* hook_copy) {
  if (nr_php_is_zval_non_empty_string(hook_copy)) {
    nr_stack_push(&NRPRG(drupal_invoke_all_hooks), hook_copy);
    nr_stack_push(&NRPRG(drupal_invoke_all_states), (void*)!NULL);
    NRPRG(check_cufa) = true;
  } else {
    nr_stack_push(&NRPRG(drupal_invoke_all_states), NULL);
  }
}

void nr_drupal_invoke_all_hook_stacks_pop() {
  if ((bool)nr_stack_pop(&NRPRG(drupal_invoke_all_states))) {
    zval* hook_copy = nr_stack_pop(&NRPRG(drupal_invoke_all_hooks));
    nr_php_arg_release(&hook_copy);
  }
  if (nr_stack_is_empty(&NRPRG(drupal_invoke_all_hooks))) {
    NRPRG(check_cufa) = false;
  }
}
#endif
