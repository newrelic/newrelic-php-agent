/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_internal_instrument.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_drupal_common.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "util_hash.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Set the Web Transaction (WT) name to "(cached page)"
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_begin it needs to be explicitly set as a before_callback to
 * ensure OAPI compatibility. This combination entails that the first wrapped
 * call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_drupal_name_wt_as_cached_page) {
  const char* buf = "(cached page)";

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  nr_txn_set_path("Drupal", NRPRG(txn), buf, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * Name the WT based on the QDrupal QForm name.
 *  txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_begin it needs to be explicitly set as a before_callback to
 * ensure OAPI compatibility. This combination entails that the first wrapped
 * call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_drupal_qdrupal_name_the_wt) {
  zval* arg1 = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_is_zval_non_empty_string(arg1)) {
    const char prefix[] = "qdrupal_qform/";
    int n = (int)sizeof(prefix);
    char* action = (char*)nr_alloca(Z_STRLEN_P(arg1) + n + 2);

    nr_strcpy(action, prefix);
    nr_strxcpy(action + n, Z_STRVAL_P(arg1), Z_STRLEN_P(arg1));

    nr_txn_set_path("QDrupal", NRPRG(txn), action, NR_PATH_TYPE_ACTION,
                    NR_NOT_OK_TO_OVERWRITE);
  } else if (0 != arg1) {
    nrl_verbosedebug(NRL_FRAMEWORK, "QDrupal: type=%d", Z_TYPE_P(arg1));
  }
  nr_php_arg_release(&arg1);
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
static void nr_drupal_http_request_ensure_second_arg(NR_EXECUTE_PROTO) {
  zval* arg = NULL;

  /*
   * If only one argument is given, an empty list is inserted as second
   * argument. NR headers are added during a later step.
   */
  if (ZEND_NUM_ARGS() == 1) {
    arg = nr_php_zval_alloc();
    array_init(arg);

    nr_php_arg_add(NR_EXECUTE_ORIG_ARGS, arg);

    nr_php_zval_free(&arg);
  }
}

static zval* nr_drupal_http_request_add_headers(NR_EXECUTE_PROTO TSRMLS_DC) {
  bool is_drupal_7;
  zval* second_arg = NULL;
  zend_execute_data* ex = nr_get_zend_execute_data(NR_EXECUTE_ORIG_ARGS);

  if (nrunlikely(NULL == ex)) {
    return NULL;
  }

  /* Ensure second argument exists in the call frame */
  nr_drupal_http_request_ensure_second_arg(NR_EXECUTE_ORIG_ARGS);

  is_drupal_7 = (ex->func->common.num_args == 2);

  /*
   * nr_php_get_user_func_arg is used, as nr_php_arg_get calls ZVAL_DUP
   * on the argument zval and thus doesn't allow us to change the
   * original argument.
   */
  second_arg = nr_php_get_user_func_arg(2, NR_EXECUTE_ORIG_ARGS);

  /*
   * Add NR headers.
   */
  nr_drupal_headers_add(second_arg, is_drupal_7 TSRMLS_CC);

  return second_arg;
}
#endif

static char* nr_drupal_http_request_get_method(NR_EXECUTE_PROTO TSRMLS_DC) {
  zval* arg2 = NULL;
  zval* arg3 = NULL;
  zval* method = NULL;
  char* http_request_method = NULL;

  /*
   * Drupal 6 will have a third argument with the method, Drupal 7 will not
   * have a third argument it must be parsed from the second.
   */
  arg3 = nr_php_arg_get(3, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  // There is no third arg, this is drupal 7
  if (NULL == arg3) {
    arg2 = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
    if (NULL != arg2) {
      method = nr_php_zend_hash_find(Z_ARRVAL_P(arg2), "method");
      if (nr_php_is_zval_valid_string(method)) {
        http_request_method
            = nr_strndup(Z_STRVAL_P(method), Z_STRLEN_P(method));
      }
    }
  } else if (nr_php_is_zval_valid_string(arg3)) {
    // This is drupal 6, the method is the third arg.
    http_request_method = nr_strndup(Z_STRVAL_P(arg3), Z_STRLEN_P(arg3));
  }
  // If the method is not set, Drupal will default to GET
  if (NULL == http_request_method) {
    http_request_method = nr_strdup("GET");
  }

  nr_php_arg_release(&arg2);
  nr_php_arg_release(&arg3);

  return http_request_method;
}

static uint64_t nr_drupal_http_request_get_response_code(
    zval** return_value TSRMLS_DC) {
  zval* code = NULL;

  if (NULL == return_value) {
    return 0;
  }

  code = nr_php_get_zval_object_property(*return_value, "code" TSRMLS_CC);
  if (NULL == code || 0 == nr_php_is_zval_non_empty_string(code)) {
    return 0;
  }

  return atoi(Z_STRVAL_P(code));
}

static char* nr_drupal_http_request_get_response_header(
    zval** return_value TSRMLS_DC) {
  zval* headers = NULL;
  zend_ulong key_num = 0;
  nr_php_string_hash_key_t* key_str = NULL;
  zval* val = NULL;

  if ((0 == NRPRG(txn)) || (0 == NRPRG(txn)->options.cross_process_enabled)) {
    return NULL;
  }

  if (NULL == return_value) {
    return NULL;
  }

  if (!nr_php_is_zval_valid_object(*return_value)) {
    return NULL;
  }

  headers = nr_php_get_zval_object_property(*return_value, "headers" TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_array(headers)) {
    return NULL;
  }

  /*
   * In Drupal 7 the header names are lowercased and in Drupal 6 they are
   * unaltered.  Therefore we do a case-insensitive lookup.
   */
  ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(headers), key_num, key_str, val) {
    (void)key_num;

    if ((NULL == key_str) || (0 == nr_php_is_zval_non_empty_string(val))) {
      continue;
    }

    if (0
        == nr_strnicmp(ZEND_STRING_VALUE(key_str),
                       NR_PSTR(X_NEWRELIC_APP_DATA))) {
      return nr_strndup(Z_STRVAL_P(val), Z_STRLEN_P(val));
    }
  }
  ZEND_HASH_FOREACH_END();

  return NULL;
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA

NR_PHP_WRAPPER(nr_drupal_http_request_before) {
  (void)wraprec;
  /*
   * For PHP 7.3 and newer, New Relic headers are added here.
   * For older versions, New Relic headers are added via the proxy function
   * nr_drupal_replace_http_request.
   *
   * Reason: using the proxy function involves swizzling
   * (nr_php_swap_user_functions), which breaks as since PHP 7.3 user
   * functions are stored in shared memory.
   */
  nr_drupal_http_request_add_headers(NR_EXECUTE_ORIG_ARGS);

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  NRPRG(drupal_http_request_depth) += 1;

  /*
   * We only want to create a metric here if this isn't a recursive call to
   * drupal_http_request() caused by the original call returning a redirect.
   * We can check how many drupal_http_request() calls are on the stack by
   * checking a counter.
   */
  if (1 == NRPRG(drupal_http_request_depth)) {
    /*
     * Parent this segment to the txn root so as to not interfere with
     * the OAPI default segment stack, which is used to dispatch to the
     * after function properly
     */
    NRPRG(drupal_http_request_segment)
        = nr_segment_start(NRPRG(txn), NULL, NULL);
    /*
     * The new segment needs to have the wraprec data attached, so that
     * fcall_end is able to properly dispatch to the after wrapper, as
     * this new segment is now at the top of the segment stack.
     */
#if ZEND_MODULE_API_NO < ZEND_8_2_X_API_NO
    NRPRG(drupal_http_request_segment)->wraprec = auto_segment->wraprec;
#else
    NRPRG(drupal_http_request_segment)->execute_data = auto_segment->execute_data;
#endif
  }
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_drupal_http_request_after) {
  zval* arg1 = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  nr_segment_external_params_t external_params
      = {.library = "Drupal",
         .uri = NULL};

  /*
   * Grab the URL for the external metric, which is the first parameter in all
   * versions of Drupal.
   */
  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_non_empty_string(arg1)) {
    goto end;
  }

  if (NULL == NR_GET_RETURN_VALUE_PTR) {
    goto end;
  }

  external_params.uri = nr_strndup(Z_STRVAL_P(arg1), Z_STRLEN_P(arg1));

  /*
   * We only want to create a metric here if this isn't a recursive call to
   * drupal_http_request() caused by the original call returning a redirect.
   * We can check how many drupal_http_request() calls are on the stack by
   * checking a counter.
   */
  if (1 == NRPRG(drupal_http_request_depth)) {
    external_params.procedure
        = nr_drupal_http_request_get_method(NR_EXECUTE_ORIG_ARGS);

    external_params.encoded_response_header
        = nr_drupal_http_request_get_response_header(NR_GET_RETURN_VALUE_PTR);

    external_params.status
        = nr_drupal_http_request_get_response_code(NR_GET_RETURN_VALUE_PTR);
    if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
      nrl_verbosedebug(
          NRL_CAT, "CAT: outbound response: transport='Drupal 6-7' %s=" NRP_FMT,
          X_NEWRELIC_APP_DATA,
          NRP_CAT(external_params.encoded_response_header));
    }
  }

end:
  if (1 == NRPRG(drupal_http_request_depth)) {
    if (external_params.uri == NULL) {
      nr_segment_discard(&NRPRG(drupal_http_request_segment));
    } else {
      nr_segment_external_end(&NRPRG(drupal_http_request_segment),
                              &external_params);
    }
    NRPRG(drupal_http_request_segment) = NULL;

    nr_free(external_params.encoded_response_header);
    nr_free(external_params.procedure);
    nr_free(external_params.uri);
  }
  nr_php_arg_release(&arg1);
  NRPRG(drupal_http_request_depth) -= 1;
}
NR_PHP_WRAPPER_END

#else

/*
 * Drupal 6:
 *   drupal_http_request ($url, $headers = array(), $method = 'GET',
 *                        $data = NULL, $retry = 3, $timeout = 30.0)
 *
 * Drupal 7:
 *   drupal_http_request ($url, array $options = array())
 *
 */
NR_PHP_WRAPPER(nr_drupal_http_request_exec) {
  zval* arg1 = NULL;
  zval** return_value = NULL;

#if ZEND_MODULE_API_NO >= ZEND_7_3_X_API_NO
  /*
   * For PHP 7.3 and newer, New Relic headers are added here.
   * For older versions, New Relic headers are added via the proxy function
   * nr_drupal_replace_http_request.
   *
   * Reason: using the proxy function involves swizzling
   * (nr_php_swap_user_functions), which breaks as since PHP 7.3 user
   * functions are stored in shared memory.
   */
  zval* arg
      = nr_drupal_http_request_add_headers(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /*
   * If an invalid argument was given for the second argument ($headers
   * or $options), the wrapped PHP function will throw a TypeError.
   */
  if (0 == nr_php_is_zval_valid_array(arg)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }
#endif /* PHP >= 7.3 */

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  NRPRG(drupal_http_request_depth) += 1;

  /*
   * Grab the URL for the external metric, which is the first parameter in all
   * versions of Drupal.
   */
  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_non_empty_string(arg1)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  return_value = NR_GET_RETURN_VALUE_PTR;

  /*
   * We only want to create a metric here if this isn't a recursive call to
   * drupal_http_request() caused by the original call returning a redirect.
   * We can check how many drupal_http_request() calls are on the stack by
   * checking a counter.
   */
  if (1 == NRPRG(drupal_http_request_depth)) {
    nr_segment_t* segment;
    nr_segment_external_params_t external_params
        = {.library = "Drupal",
           .uri = nr_strndup(Z_STRVAL_P(arg1), Z_STRLEN_P(arg1))};

    external_params.procedure
        = nr_drupal_http_request_get_method(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

    segment = nr_segment_start(NRPRG(txn), NULL, NULL);

    /*
     * Our wrapper for drupal_http_request (which we installed in
     * nr_drupal_replace_http_request()) will take care of adding the request
     * headers, so let's just go ahead and call the function.
     */
    NR_PHP_WRAPPER_CALL;

    external_params.encoded_response_header
        = nr_drupal_http_request_get_response_header(return_value TSRMLS_CC);

    external_params.status
        = nr_drupal_http_request_get_response_code(return_value TSRMLS_CC);
    if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
      nrl_verbosedebug(
          NRL_CAT, "CAT: outbound response: transport='Drupal 6-7' %s=" NRP_FMT,
          X_NEWRELIC_APP_DATA,
          NRP_CAT(external_params.encoded_response_header));
    }

    nr_segment_external_end(&segment, &external_params);

    nr_free(external_params.encoded_response_header);
    nr_free(external_params.procedure);
    nr_free(external_params.uri);
  } else {
    NR_PHP_WRAPPER_CALL;
  }

end:
  nr_php_arg_release(&arg1);
  NRPRG(drupal_http_request_depth) -= 1;
}
NR_PHP_WRAPPER_END

#endif

static void nr_drupal_name_the_wt(const zend_function* func TSRMLS_DC) {
  char* action = NULL;

  if ((NULL == func) || (NULL == nr_php_function_name(func))) {
    return;
  }

  action = nr_strndup(nr_php_function_name(func),
                      (int)nr_php_function_name_length(func));

  nr_txn_set_path("Drupal", NRPRG(txn), action, NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  nr_free(action);
}

/*
 * Purpose : Wrap the given function using the current module_invoke_all()
 *           context (encapsulated within the drupal_module_invoke_all_hook
 *           and drupal_module_invoke_all_hook_len per request global fields).
 */
static void nr_drupal_wrap_hook_within_module_invoke_all(
    const zend_function* func TSRMLS_DC) {
  nr_status_t rv;
  char* module = NULL;
  size_t module_len = 0;

  /*
   * Since this function is only called if the immediate caller is
   * module_invoke_all(), the drupal_module_invoke_all_hook global should be
   * available.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  zval* curr_hook
      = (zval*)nr_stack_get_top(&NRPRG(drupal_invoke_all_hooks));
  if (!nr_php_is_zval_non_empty_string(curr_hook)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: cannot extract hook name from global stack",
                     __func__);
    return;
  }
  char* hook_name = Z_STRVAL_P(curr_hook);
  size_t hook_len = Z_STRLEN_P(curr_hook);
#else
  char* hook_name = NRPRG(drupal_invoke_all_hook);
  size_t hook_len = NRPRG(drupal_invoke_all_hook_len);
#endif
  if (NULL == hook_name) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: cannot extract module name without knowing the hook",
                     __func__);
    return;
  }

  rv = module_invoke_all_parse_module_and_hook(&module, &module_len, hook_name,
                                               hook_len, func);

  if (NR_SUCCESS != rv) {
    return;
  }

  nr_php_wrap_user_function_drupal(nr_php_function_name(func),
                                   nr_php_function_name_length(func), module,
                                   module_len, hook_name, hook_len TSRMLS_CC);

  nr_free(module);
}

/*
 * Purpose : Wrap calls to call_user_func_array for two reasons identified
 *           by specific call stacks.
 *
 * Transaction naming:
 *
 *   1. call_user_func_array
 *   2. menu_execute_active_handler
 *
 * Module/Hook metric generation:
 *
 *   1. call_user_func_array
 *   2. module_invoke_all
 *
 */
static void nr_drupal_call_user_func_array_callback(zend_function* func,
                                                    const zend_function* caller
                                                        NRUNUSED TSRMLS_DC) {
  const char* caller_name = NULL;

  if (nrunlikely(NULL == caller)) {
    return;
  }

  if (!nr_drupal_is_framework(NRPRG(current_framework))) {
    return;
  }

  caller_name = nr_php_function_name(caller);

  /*
   * If the caller was module_invoke_all, then perform hook/module
   * instrumentation. This caller is checked first, since it occurs most
   * frequently.
   */
  if (NRINI(drupal_modules)) {
    if (0 == nr_strncmp(caller_name, NR_PSTR("module_invoke_all"))) {
      nr_drupal_wrap_hook_within_module_invoke_all(func TSRMLS_CC);
      return;
    }
  }

  if (0 == nr_strncmp(caller_name, NR_PSTR("menu_execute_active_handler"))) {
    nr_drupal_name_the_wt(func TSRMLS_CC);
  }
}

/*
 * Purpose : Wrap view::execute in order to create Drupal Views metrics.
 */
NR_PHP_WRAPPER(nr_drupal_wrap_view_execute) {
  zval* this_var = NULL;
  zval* name_property = NULL;
  int name_len;
  char* name = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (!nr_php_is_zval_valid_object(this_var)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  name_property = nr_php_get_zval_object_property(this_var, "name" TSRMLS_CC);
  if (0 == nr_php_is_zval_non_empty_string(name_property)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  name_len = Z_STRLEN_P(name_property);
  name = nr_strndup(Z_STRVAL_P(name_property), name_len);

  zcaught = nr_drupal_do_view_execute(name, name_len, auto_segment,
                                      NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  was_executed = 1;

leave:
  nr_free(name);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_drupal_cron_run) {
  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  nr_txn_set_as_background_job(NRPRG(txn), "drupal_cron_run called");

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
static void nr_drupal_replace_http_request(TSRMLS_D) {
  zend_function* orig = NULL;
  zend_function* wrapper = NULL;

  orig = nr_php_find_function("drupal_http_request" TSRMLS_CC);
  wrapper = nr_php_find_function("newrelic_drupal_http_request" TSRMLS_CC);

  /*
   * Add a function that will replace drupal_http_request() and ensure that we
   * add our request headers for CAT.
   *
   * There is an oddity in here: this function looks like it makes a recursive
   * call to newrelic_drupal_http_request(), but in fact that will be the
   * original drupal_http_request(), as we'll swap the implementations.
   *
   * We can't do this until the original drupal_http_request() is defined,
   * which may not be the case immediately if the framework has been forced.
   */
  if (orig && !wrapper) {
    int argc = (int)orig->common.num_args;

    /*
     * Drupal 6 and 7 have slightly different APIs, so we'll use different
     * wrappers for each. This is slightly tricky in practice. The Pressflow
     * fork of Drupal 6 has backported features from Drupal 7 that cause the
     * agent to detect it as Drupal 7 rather than Drupal 6. Therefore, the
     * detected framework version can't be used to determine which variant
     * of drupal_http_request to replace. Instead, differentiate the two
     * variants based on their function signatures.
     */
    if (6 == argc) { /* The Drupal 6 variant accepts six arguments. */
      int retval;

      retval = zend_eval_string(
          "function newrelic_drupal_http_request($url, $headers = array(), "
          "$method = 'GET', $data = null, $retry = 3, $timeout = 30.0) {"
          "  $metadata = newrelic_get_request_metadata('Drupal 6');"
          "  if (is_array($headers)) {"
          "    $headers = array_merge($headers, $metadata);"
          "  } elseif (is_null($headers)) {"
          "    $headers = $metadata;"
          "  }"
          "  $result = newrelic_drupal_http_request($url, $headers, $method, "
          "$data, $retry, $timeout);"
          "  return $result;"
          "}",
          NULL, "newrelic/drupal6" TSRMLS_CC);

      if (SUCCESS != retval) {
        nrl_warning(NRL_FRAMEWORK, "%s: error evaluating Drupal 6 code",
                    __func__);
      }
    } else if (2 == argc) { /* The Drupal 7 variant accepts two arguments. */
      int retval;

      retval = zend_eval_string(
          "function newrelic_drupal_http_request($url, array $options = "
          "array()) {"
          "  $metadata = newrelic_get_request_metadata('Drupal 7');"
          /*
           * array_key_exists() is used instead of isset() because isset()
           * will return false if $options['headers'] exists but is null. As
           * noted below, we need to pass the value through and not
           * accidentally set it to a valid value.
           */
          "  if (array_key_exists('headers', $options)) {"
          "    if (is_array($options['headers'])) {"
          "      $options['headers'] += $metadata;"
          "    }"
          /*
           * We do nothing here if $options['headers'] is set but invalid
           * (ie not an array) because drupal_http_request() will generate
           * an "unsupported operand types" fatal error that we don't want
           * to squash by accident (since we don't want to change
           * behaviour).
           */
          "  } else {"
          "    $options['headers'] = $metadata;"
          "  }"
          "  $result = newrelic_drupal_http_request($url, $options);"
          "  return $result;"
          "}",
          NULL, "newrelic/drupal7" TSRMLS_CC);

      if (SUCCESS != retval) {
        nrl_warning(NRL_FRAMEWORK, "%s: error evaluating Drupal 7 code",
                    __func__);
      }
    } else {
      nrl_info(NRL_FRAMEWORK,
               "%s: unable to determine drupal_http_request"
               " variant: num_args=%d",
               __func__, argc);
    }

    wrapper = nr_php_find_function("newrelic_drupal_http_request" TSRMLS_CC);
    nr_php_swap_user_functions(orig, wrapper);
  }
}
#endif

NR_PHP_WRAPPER(nr_drupal_wrap_module_invoke) {
  int module_len;
  int hook_len;
  char* module = NULL;
  char* hook = NULL;
  zval* arg1 = NULL;
  zval* arg2 = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  arg1 = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  arg2 = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if ((0 == nr_php_is_zval_non_empty_string(arg1))
      || (0 == nr_php_is_zval_non_empty_string(arg2))) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  module_len = Z_STRLEN_P(arg1);
  module = nr_strndup(Z_STRVAL_P(arg1), module_len);
  hook_len = Z_STRLEN_P(arg2);
  hook = nr_strndup(Z_STRVAL_P(arg2), hook_len);

  NR_PHP_WRAPPER_CALL;

  nr_drupal_create_metric(auto_segment, NR_PSTR(NR_DRUPAL_MODULE_PREFIX),
                          module, module_len);
  nr_drupal_create_metric(auto_segment, NR_PSTR(NR_DRUPAL_HOOK_PREFIX), hook,
                          hook_len);

leave:
  nr_free(module);
  nr_free(hook);
  nr_php_arg_release(&arg1);
  nr_php_arg_release(&arg2);
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
NR_PHP_WRAPPER(nr_drupal_wrap_module_invoke_all_before) {
  (void)wraprec;
  zval* hook_copy = NULL;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  hook_copy = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS);
  nr_drupal_invoke_all_hook_stacks_push(hook_copy);
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_drupal_wrap_module_invoke_all_after) {
  (void)wraprec;
  nr_drupal_invoke_all_hook_stacks_pop();
}
NR_PHP_WRAPPER_END

#else
NR_PHP_WRAPPER(nr_drupal_wrap_module_invoke_all) {
  zval* hook = NULL;
  char* prev_hook = NULL;
  nr_string_len_t prev_hook_len;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL);

  hook = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_non_empty_string(hook)) {
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  prev_hook = NRPRG(drupal_invoke_all_hook);
  prev_hook_len = NRPRG(drupal_invoke_all_hook_len);
  NRPRG(drupal_invoke_all_hook)
      = nr_strndup(Z_STRVAL_P(hook), Z_STRLEN_P(hook));
  NRPRG(drupal_invoke_all_hook_len) = Z_STRLEN_P(hook);
  NRPRG(check_cufa) = true;

  NR_PHP_WRAPPER_CALL;

  nr_free(NRPRG(drupal_invoke_all_hook));
  NRPRG(drupal_invoke_all_hook) = prev_hook;
  NRPRG(drupal_invoke_all_hook_len) = prev_hook_len;
  if (NULL == NRPRG(drupal_invoke_all_hook)) {
    NRPRG(check_cufa) = false;
  }

leave:
  nr_php_arg_release(&hook);
}
NR_PHP_WRAPPER_END
#endif /* OAPI */

/*
 * Enable the drupal instrumentation.
 */
void nr_drupal_enable(TSRMLS_D) {
  nr_php_add_call_user_func_array_pre_callback(
      nr_drupal_call_user_func_array_callback TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("drupal_cron_run"),
                            nr_drupal_cron_run TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("QFormBase::Run"), nr_drupal_qdrupal_name_the_wt, NULL);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("drupal_page_cache_header"), nr_drupal_name_wt_as_cached_page,
      NULL);
  nr_php_wrap_user_function_before_after(
      NR_PSTR("drupal_http_request"), nr_drupal_http_request_before,
      nr_drupal_http_request_after);
#else
  nr_php_wrap_user_function(NR_PSTR("QFormBase::Run"),
                            nr_drupal_qdrupal_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("drupal_page_cache_header"),
                            nr_drupal_name_wt_as_cached_page TSRMLS_CC);
  nr_php_wrap_user_function(NR_PSTR("drupal_http_request"),
                            nr_drupal_http_request_exec TSRMLS_CC);
#endif

  /*
   * The drupal_modules config setting controls instrumentation of modules,
   * hooks, and views.
   */
  if (NRINI(drupal_modules)) {
    nr_php_wrap_user_function(NR_PSTR("module_invoke"),
                              nr_drupal_wrap_module_invoke TSRMLS_CC);
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
    nr_php_wrap_user_function_before_after(
        NR_PSTR("module_invoke_all"), nr_drupal_wrap_module_invoke_all_before,
        nr_drupal_wrap_module_invoke_all_after);
#else
    nr_php_wrap_user_function(NR_PSTR("module_invoke_all"),
                              nr_drupal_wrap_module_invoke_all TSRMLS_CC);
#endif /* OAPI */
    nr_php_wrap_user_function(NR_PSTR("view::execute"),
                              nr_drupal_wrap_view_execute TSRMLS_CC);
  }

#if ZEND_MODULE_API_NO < ZEND_7_3_X_API_NO
  /*
   * For PHP 7.3 and newer, NR headers are added directly in
   * nr_drupal_http_request_exec. For older versions, New Relic headers
   * are added via the proxy function nr_drupal_replace_http_request.
   */
  nr_php_user_function_add_declared_callback(
      NR_PSTR("drupal_http_request"), nr_drupal_replace_http_request TSRMLS_CC);
#endif
}
