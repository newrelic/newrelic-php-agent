/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_drupal_common.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "fw_symfony_common.h"
#include "nr_txn.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

/*
 * Purpose : Convenience function to handle adding a callback to a method,
 *           given a class entry and a method name. This will check the
 *           fn_flags to see if the zend_function has previously been
 *           instrumented, thereby circumventing the need to walk over the
 *           linked list of wraprecs if so.
 *
 * Params  : 1. The class entry.
 *           2. The method name.
 *           3. The length of the method name.
 *           4. The callback.
 */
static void nr_drupal8_add_method_callback(const zend_class_entry* ce,
                                           const char* method,
                                           size_t method_len,
                                           nrspecialfn_t callback TSRMLS_DC) {
  zend_function* function = NULL;

  if (NULL == ce) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Drupal 8: got NULL class entry in %s",
                     __func__);
    return;
  }

  function = nr_php_find_class_method(ce, method);
  if (NULL == function) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Drupal 8+: cannot get zend_function entry for %.*s::%.*s",
                     NRSAFELEN(nr_php_class_entry_name_length(ce)),
                     nr_php_class_entry_name(ce), NRSAFELEN(method_len),
                     method);
    return;
  }

  /*
   * Using nr_php_op_array_get_wraprec to check if the method has valid
   * instrumentation.
   */
#if ZEND_MODULE_API_NO < ZEND_7_4_X_API_NO
  if (NULL == nr_php_op_array_get_wraprec(&function->op_array TSRMLS_CC)) {
#else
  if (NULL == nr_php_get_wraprec(function)) {
#endif
    char* class_method = nr_formatf(
        "%.*s::%.*s", NRSAFELEN(nr_php_class_entry_name_length(ce)),
        nr_php_class_entry_name(ce), NRSAFELEN(method_len), method);

    nr_php_wrap_user_function(class_method, nr_strlen(class_method),
                              callback TSRMLS_CC);

    nr_free(class_method);
  }
}

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
static void nr_drupal8_add_method_callback_before_after(
                                           const zend_class_entry* ce,
                                           const char* method,
                                           size_t method_len,
                                           nrspecialfn_t before_callback,
                                           nrspecialfn_t after_callback) {
  zend_function* function = NULL;

  if (NULL == ce) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Drupal 8: got NULL class entry in %s",
                     __func__);
    return;
  }

  function = nr_php_find_class_method(ce, method);
  if (NULL == function) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Drupal 8+: cannot get zend_function entry for %.*s::%.*s",
                     NRSAFELEN(nr_php_class_entry_name_length(ce)),
                     nr_php_class_entry_name(ce), NRSAFELEN(method_len),
                     method);
    return;
  }

  if (NULL == nr_php_get_wraprec(function)) {
    char* class_method = nr_formatf(
        "%.*s::%.*s", NRSAFELEN(nr_php_class_entry_name_length(ce)),
        nr_php_class_entry_name(ce), NRSAFELEN(method_len), method);

    nr_php_wrap_user_function_before_after(
                              class_method, nr_strlen(class_method),
                              before_callback, after_callback);

    nr_free(class_method);
  }
}
#endif // OAPI

/*
 * Purpose : Check if the given function or method is in the current call
 *           stack.
 *
 * Params  : 1. The name of the function or method to search for.
 *           2. An optional class name to look for.
 *
 * Returns : 1 if the function or method is found, 0 otherwise.
 */
static int nr_drupal8_is_function_in_call_stack(const char* function,
                                                const char* scope TSRMLS_DC) {
  int found = 0;
  zval* frame = NULL;
  zval* trace = NULL;

  if (NULL == function) {
    nrl_error(NRL_TXN, "%s: function should never be NULL!", __func__);
    return 0;
  }

  trace = nr_php_zval_alloc();

  /* Grab the actual backtrace. */
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  zend_fetch_debug_backtrace(trace, 0, 1, 0 TSRMLS_CC);
#else /* PHP < 5.4 */
  zend_fetch_debug_backtrace(trace, 0, 1 TSRMLS_CC);
#endif

  if (!nr_php_is_zval_valid_array(trace)) {
    nrl_error(NRL_TXN, "%s: trace should never not be an array", __func__);
    nr_php_zval_free(&trace);
    return 0;
  }

  /*
   * Now walk the stack frames and see if any match.
   */
  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(trace), frame) {
    zval* frame_func = NULL;
    zval* frame_scope = NULL;

    if (!nr_php_is_zval_valid_array(frame)) {
      nrl_verbosedebug(NRL_TXN, "%s: unexpected non-array frame in trace",
                       __func__);
      continue;
    }

    frame_func = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "function");
    frame_scope = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "class");

    if (nr_php_is_zval_valid_string(frame_func)) {
      if ((NULL == scope) && (0 == nr_php_is_zval_valid_string(frame_scope))) {
        /* It's a standard function, not a method. */
        if (0
            == nr_strnicmp(function, Z_STRVAL_P(frame_func),
                           Z_STRLEN_P(frame_func))) {
          found = 1;
          goto out;
        }
      } else if (scope && nr_php_is_zval_valid_string(frame_scope)) {
        /* Looking for a method, and this is a method. */
        if ((0
             == nr_strnicmp(function, Z_STRVAL_P(frame_func),
                            Z_STRLEN_P(frame_func)))
            && (0
                == nr_strnicmp(scope, Z_STRVAL_P(frame_scope),
                               Z_STRLEN_P(frame_scope)))) {
          found = 1;
          goto out;
        }
      }
    }
  }
  ZEND_HASH_FOREACH_END();

out:
  nr_php_zval_free(&trace);
  return found;
}

/*
 * Purpose : Name the Drupal 8 transaction based on the return value of
 *           ControllerResolver::getControllerFromDefinition().
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_NOT_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped
 * function in func_end no change is needed to ensure OAPI compatibility as it
 * will use the default func_end after callback. This entails that the last
 * wrapped call gets to name the txn which corresponds to the naming details
 * within the function.
 */
NR_PHP_WRAPPER(nr_drupal8_name_the_wt) {
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);
  NR_PHP_WRAPPER_CALL;

  if (retval_ptr && *retval_ptr) {
    zval* retval = *retval_ptr;

    /*
     * Note that the name returned from nr_php_callable_to_string may be
     * suboptimal for anonymous functions, closures and generators. It doesn't
     * appear that Drupal 8 has a way to define any of those as controllers at
     * present, but should this be added, it may cause MGI. We would likely
     * want to change from using the generated class name to using a name
     * synthesised from the definition file and line of the callable.
     */
    char* name = nr_php_callable_to_string(retval TSRMLS_CC);

    /*
     * Drupal 8 has a concept of title callbacks, which are controllers
     * attached to other controllers that return the page title. We don't want
     * to consider these for the purposes of transaction naming.
     */
    if ((NULL != name)
        && (0
            == nr_drupal8_is_function_in_call_stack(
                "getTitle",
                "Drupal\\Core\\Controller\\TitleResolver" TSRMLS_CC))) {
      /*
       * This is marked as OK to overwrite because we generally want the last
       * controller. Drupal 8 will often start by invoking a very general
       * controller, such as
       * Drupal\Core\Controller\HtmlPageController->content, before delegating
       * control to the real controller.
       */
      nr_txn_set_path("Drupal8", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                      NR_NOT_OK_TO_OVERWRITE);
    }

    nr_free(name);
  }
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the last wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_drupal8_name_the_wt_cached) {
  const char* name = "page_cache";
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);
  NR_PHP_WRAPPER_CALL;

  /*
   * Drupal\page_cache\StackMiddleware\PageCache::get returns a
   * Symfony\Component\HttpFoundation\Response if there is a cache hit and false
   * otherwise.
   */
  if (retval_ptr && nr_php_is_zval_valid_object(*retval_ptr)) {
    nr_txn_set_path("Drupal8", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Wrap Drupal\views\ViewExecutable::execute in order to create
 *           Drupal Views metrics.
 */
NR_PHP_WRAPPER(nr_drupal8_wrap_view_execute) {
  zval* this_var = NULL;
  zval* storage = NULL;
  zval* label = NULL;
  int name_len;
  char* name = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(this_var)) {
    goto leave;
  }

  storage = nr_php_get_zval_object_property(this_var, "storage" TSRMLS_CC);
  if ((NULL == storage)
      || (!nr_php_object_instanceof_class(
          storage, "Drupal\\views\\Entity\\View" TSRMLS_CC))) {
    nrl_verbosedebug(
        NRL_FRAMEWORK,
        "Drupal 8: ViewExecutable storage property isn't a View object");
    goto leave;
  }

  label = nr_php_call(storage, "label");
  if (0 == nr_php_is_zval_non_empty_string(label)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Drupal 8: View::label() didn't return a string");
    goto leave;
  }

  name_len = Z_STRLEN_P(label);
  name = nr_strndup(Z_STRVAL_P(label), name_len);

  zcaught = nr_drupal_do_view_execute(name, name_len, auto_segment,
                                      NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  was_executed = 1;

  nr_free(name);

leave:
  nr_php_zval_free(&label);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Iterator function to walk over an array of module names
 *           implementing a particular hook and instrument those hooks.
 *
 * Params  : 1. The module name.
 *         : 2. The hook name.
 *
 * Returns : ZEND_HASH_APPLY_KEEP.
 */
static int nr_drupal8_apply_hook(zval* element,
                                 const zval* hook,
                                 zend_hash_key* key NRUNUSED TSRMLS_DC) {
  if (nr_php_is_zval_non_empty_string(element)) {
    nr_drupal_hook_instrument(Z_STRVAL_P(element), Z_STRLEN_P(element),
                              Z_STRVAL_P(hook), Z_STRLEN_P(hook) TSRMLS_CC);
  }

  return ZEND_HASH_APPLY_KEEP;
}

/*
 * Purpose : A post callback to handle a
 *           ModuleHandlerInterface::getImplementations() call and ensure that
 *           all returned modules have instrumentation for the hook in
 *           question.
 */
NR_PHP_WRAPPER(nr_drupal8_post_get_implementations) {
  zval* hook = NULL;
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  hook = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  NR_PHP_WRAPPER_CALL;

  /*
   * The return value is expected to be an array of modules that implement the
   * hook that was given as the first parameter to getImplementations(). We
   * want to iterate over those modules and instrument each hook function.
   */
  if (retval_ptr && nr_php_is_zval_valid_array(*retval_ptr)) {
    if (nr_php_is_zval_non_empty_string(hook)) {
      nr_php_zend_hash_zval_apply(Z_ARRVAL_P(*retval_ptr),
                                  (nr_php_zval_apply_t)nr_drupal8_apply_hook,
                                  hook TSRMLS_CC);
    }
  }

  nr_php_arg_release(&hook);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : A post callback to handle a
 *           ModuleHandlerInterface::implementsHook() call and ensure that the
 *           relevant hook function is instrumented.
 */
NR_PHP_WRAPPER(nr_drupal8_post_implements_hook) {
  zval* hook = NULL;
  zval* module = NULL;
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  hook = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  module = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  NR_PHP_WRAPPER_CALL;

  /*
   * The module and hook are provided as the parameters to implementsHook(): if
   * it returned true, then they're a valid module and hook, and we should
   * instrument accordingly.
   */
  if (retval_ptr && nr_php_is_zval_true(*retval_ptr)) {
    if (nr_php_is_zval_non_empty_string(module)
        && nr_php_is_zval_non_empty_string(hook)) {
      nr_drupal_hook_instrument(Z_STRVAL_P(module), Z_STRLEN_P(module),
                                Z_STRVAL_P(hook), Z_STRLEN_P(hook) TSRMLS_CC);
    }
  }

  nr_php_arg_release(&hook);
  nr_php_arg_release(&module);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Handles ModuleHandlerInterface::invokeAllWith()'s callback
 *           and ensure that the relevant module_hook function is instrumented.
 */
NR_PHP_WRAPPER(nr_drupal94_invoke_all_with_callback) {
  zval* module = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);
  module = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_non_empty_string(module)) {
    goto leave;
  }

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  zval* curr_hook
      = (zval*)nr_stack_get_top(&NRPRG(drupal_invoke_all_hooks));
  if (UNEXPECTED(!nr_php_is_zval_non_empty_string(curr_hook))) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: cannot extract hook name from global stack",
                     __func__);
    goto leave;
  }
  nr_drupal_hook_instrument(Z_STRVAL_P(module), Z_STRLEN_P(module),
                            Z_STRVAL_P(curr_hook), Z_STRLEN_P(curr_hook));
#else
  nr_drupal_hook_instrument(Z_STRVAL_P(module), Z_STRLEN_P(module),
                            NRPRG(drupal_invoke_all_hook),
                            NRPRG(drupal_invoke_all_hook_len) TSRMLS_CC);
#endif // OAPI

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&module);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Handles ModuleHandlerInterface::invokeAllWith() call and ensure
 * that the relevant hook function is instrumented. At this point in the call
 *           stack, we do not know which module to instrument, so we
 *           must first wrap the callback passed into this function
 */
NR_PHP_WRAPPER(nr_drupal94_invoke_all_with) {
  zval* callback = NULL;
  zval* hook = NULL;
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  char* prev_hook = NULL;
  int prev_hook_len;
#endif // not OAPI

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  hook = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_non_empty_string(hook)) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
    nr_php_arg_release(&hook);
#endif // OAPI
    goto leave;
  }

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_drupal_invoke_all_hook_stacks_push(hook);
#else
  prev_hook = NRPRG(drupal_invoke_all_hook);
  prev_hook_len = NRPRG(drupal_invoke_all_hook_len);
  NRPRG(drupal_invoke_all_hook)
      = nr_strndup(Z_STRVAL_P(hook), Z_STRLEN_P(hook));
  NRPRG(drupal_invoke_all_hook_len) = Z_STRLEN_P(hook);
  NRPRG(check_cufa) = true;
#endif // OAPI
  callback = nr_php_arg_get(2, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /* This instrumentation will fail if callback has already been wrapped
   * with a special instrumentation callback in a different context.
   * In this scenario, we will be unable to instrument hooks and modules for
   * this particular call */
  nr_php_wrap_generic_callable(callback,
                               nr_drupal94_invoke_all_with_callback TSRMLS_CC);
  NR_PHP_WRAPPER_CALL;

  nr_php_arg_release(&callback);
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_free(NRPRG(drupal_invoke_all_hook));
  NRPRG(drupal_invoke_all_hook) = prev_hook;
  NRPRG(drupal_invoke_all_hook_len) = prev_hook_len;
  if (NULL == NRPRG(drupal_invoke_all_hook)) {
    NRPRG(check_cufa) = false;
  }
#endif // not OAPI

leave: ;
#if ZEND_MODULE_API_NO < ZEND_8_0_X_API_NO \
    || defined OVERWRITE_ZEND_EXECUTE_DATA
  /* for OAPI, the _after callback handles this free */
  nr_php_arg_release(&hook);
#endif // not OAPI
}
NR_PHP_WRAPPER_END

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
NR_PHP_WRAPPER(nr_drupal94_invoke_all_with_after) {
  (void)wraprec;
  nr_drupal_invoke_all_hook_stacks_pop();
}
NR_PHP_WRAPPER_END
#endif // OAPI

/*
 * Purpose : Wrap the invoke() method of the module handler instance in use.
 */
NR_PHP_WRAPPER(nr_drupal8_module_handler) {
  zend_class_entry* ce = NULL;
  zval** retval_ptr = NR_GET_RETURN_VALUE_PTR;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  NR_PHP_WRAPPER_CALL;

  if (NULL == retval_ptr) {
    NR_PHP_WRAPPER_LEAVE;
  }

  if (0
      == nr_php_object_instanceof_class(
          *retval_ptr,
          "Drupal\\Core\\Extension\\ModuleHandlerInterface" TSRMLS_CC)) {
    NR_PHP_WRAPPER_LEAVE;
  }

  ce = Z_OBJCE_P(*retval_ptr);

  nr_drupal8_add_method_callback(ce, NR_PSTR("getimplementations"),
                                 nr_drupal8_post_get_implementations TSRMLS_CC);
  nr_drupal8_add_method_callback(ce, NR_PSTR("implementshook"),
                                 nr_drupal8_post_implements_hook TSRMLS_CC);
  /* Drupal 9.4 introduced a replacement method for getImplentations */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_drupal8_add_method_callback_before_after(
                                 ce, NR_PSTR("invokeallwith"),
                                 nr_drupal94_invoke_all_with,
                                 nr_drupal94_invoke_all_with_after);
#else
  nr_drupal8_add_method_callback(ce, NR_PSTR("invokeallwith"),
                                 nr_drupal94_invoke_all_with TSRMLS_CC);
#endif
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This entails that the last wrapped call gets to name the
 * txn but it is overwritable if another better name comes along.
 */
NR_PHP_WRAPPER(nr_drupal8_name_the_wt_via_symfony) {
  zval* event = NULL;
  zval* request = NULL;
  zval* controller = NULL;

  /* Warning avoidance. */
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_DRUPAL8);

  /*
   * See nr_symfony2_name_the_wt in fw_symfony2.c for a more detailed
   * explanation of this logic.
   */

  event = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(event)) {
    nrl_verbosedebug(
        NRL_TXN,
        "Drupal 8 via Symfony: RouterListener::onKernelRequest() does not "
        "have an event parameter");
    goto end;
  }

  /* Get the request object from the event. */
  request = nr_php_call(event, "getRequest");
  if (false
      == nr_php_object_instanceof_class(
          request, "Symfony\\Component\\HttpFoundation\\Request" TSRMLS_CC)) {
    nrl_verbosedebug(
        NRL_TXN,
        "Drupal 8 via Symfony: GetResponseEvent::getRequest() returned a "
        "non-Request object");
    goto end;
  }

  controller = nr_symfony_object_get_string(request, "_controller" TSRMLS_CC);

  if (nrlikely(nr_php_is_zval_non_empty_string(controller))) {
    nr_txn_set_path("Drupal8", NRPRG(txn), Z_STRVAL_P(controller),
                    NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
  } else {
    nrl_verbosedebug(NRL_TXN, "Drupal 8 via Symfony: No _controller is set");
  }

end:
  nr_php_arg_release(&event);
  nr_php_zval_free(&request);
  nr_php_zval_free(&controller);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * Drupal stores the version of the framework in the class constant
 * Drupal::VERSION. This code first verifies the 'Drupal' class exists (note
 * having to pass the lower case name of the class). If present then an attempt
 * is made to retrieve the 'VERSION' class constant. Both of these checks rely
 * on existing "nr_" routines that have been designed to be robust and will not
 * cause an issue in user's application if either check were to fail.
 */
void nr_drupal_version() {
  zval* zval_version = NULL;
  zend_class_entry* class_entry = NULL;

  class_entry = nr_php_find_class("drupal");
  if (NULL == class_entry) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: 'Drupal' class not found", __func__);
    return;
  }

  zval_version = nr_php_get_class_constant(class_entry, "VERSION");
  if (NULL == zval_version) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: Drupal does not have VERSION",
                     __func__);
    return;
  }

  // Add php package to transaction
  if (nr_php_is_zval_valid_string(zval_version)) {
    char* version = Z_STRVAL_P(zval_version);
    nr_txn_add_php_package(NRPRG(txn), "drupal/core", version);
  }

  nr_php_zval_free(&zval_version);
}

void nr_drupal8_enable(TSRMLS_D) {
  /*
   * Obtain a transation name if a page was cached.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Drupal\\page_cache\\StackMiddleware\\PageCache::get"),
      nr_drupal8_name_the_wt_cached TSRMLS_CC);

  /*
   * Drupal 8 uses Symfony 2 under the hood. Thus we try to hook into
   * the Symfony RouterListener to determine the main controller this
   * request is routed through.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Symfony\\Component\\HttpKernel\\EventListe"
              "ner\\RouterListener::onKernelRequest"),
      nr_drupal8_name_the_wt_via_symfony, NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("Symfony\\Component\\HttpKernel\\EventListe"
                                    "ner\\RouterListener::onKernelRequest"),
                            nr_drupal8_name_the_wt_via_symfony TSRMLS_CC);
#endif

  /*
   * The ControllerResolver is the legacy way to name
   * Drupal 8 transactions and is left here as a fallback. It won't
   * overwrite transaction names set via the RouterListener callback
   * above, but kicks in for use cases where the RouterListener is not
   * involved.
   */
  nr_php_wrap_user_function(NR_PSTR("Drupal\\Core\\Controller\\ControllerResolv"
                                    "er::getControllerFromDefinition"),
                            nr_drupal8_name_the_wt TSRMLS_CC);

  /*
   * The drupal_modules config setting controls instrumentation of modules,
   * hooks, and views.
   */
  if (NRINI(drupal_modules)) {
    /*
     * We actually need to wrap some methods of the module handler
     * implementation to generate module metrics, but we can't simply wrap
     * ModuleHandler::invoke() because Drupal 8 allows for this to be replaced
     * by anything that implements ModuleHandlerInterface. Instead, we'll catch
     * the return value of Drupal::moduleHandler(), which is the module handler
     * instance actually in use, and instrument that in
     * nr_drupal8_post_module_handler().
     */
    nr_php_wrap_user_function(NR_PSTR("Drupal::moduleHandler"),
                              nr_drupal8_module_handler TSRMLS_CC);

    /*
     * View metrics also have be handled in a Drupal 8 specific manner due to
     * the naming mechanism for views changing significantly from previous
     * versions.
     */
    nr_php_wrap_user_function(NR_PSTR("Drupal\\views\\ViewExecutable::execute"),
                              nr_drupal8_wrap_view_execute TSRMLS_CC);
  }

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), "drupal/core",
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
