/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_php_packages.h"
#include "nr_txn.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_error.h"
#include "php_execute.h"
#include "php_globals.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "php_hash.h"
#include "fw_laravel.h"
#include "fw_laravel_queue.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

#include "ext/standard/php_versioning.h"
#include "Zend/zend_exceptions.h"

/*
 * This instruments Laravel 4.0-5.0, inclusive.
 * There is no support for Laravel 3.X or earlier.
 *
 * The first round of support was done for Laravel 4.1 (Jan 2014), thinking
 * that what worked for Laravel 4.1 would work for Laravel 4.0.
 *
 * This proved to not be the case, as significant changes were made in the
 * Routing code going from 4.0 to 4.1, and it is the Routing code that we hook.
 *
 * Known issue: users who have replaced the router service with code that
 * doesn't call Router::callGlobalFilter() (for Laravel 4.0) or
 * Router::dispatchToRoute() (for Laravel 4.1 and later) _and_ have disabled
 * filtering will not get naming without adding PHP code that calls
 * newrelic_name_transaction().
 */

/*
 * Forward declarations of useful static functions.
 */

/*
 * Purpose : Given a router service that looks at least a little like Laravel's
 *           default and a request object, attempt to name the transction.
 *
 * Params  : 1. The router service.
 *           2. The request object.
 */
static void nr_laravel_name_transaction(zval* router, zval* request TSRMLS_DC);

/*
 * Purpose : Given a transaction, decides if it's OK to go ahead and assign
 *           the $METHOD/index.php name **or** if we should skip assigning
 *           that name because a previous call to nr_laravel_name_transaction
 *           has already assigned a better name.
 *
 * Params  : 1. The router service.
 *           2. The request object.
 */
static nr_status_t nr_laravel_should_assign_generic_path(const nrtxn_t* txn,
                                                         zval* request
                                                             TSRMLS_DC);

/*
 * We define a class called newrelic\Laravel\AfterFilter here. This is an
 * invokable class (like a closure) that captures the Laravel Application
 * object and can be used as a callback by the router filtering mechanism for
 * transaction naming.
 */
zend_class_entry* nr_laravel_afterfilter_ce;

ZEND_BEGIN_ARG_INFO_EX(nr_laravel_afterfilter_construct_arginfo, 0, 0, 1)
ZEND_ARG_INFO(0, app)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(nr_laravel_afterfilter_invoke_arginfo, 0, 0, 2)
ZEND_ARG_INFO(0, request)
ZEND_ARG_INFO(0, response)
ZEND_END_ARG_INFO()

/*
 * AfterFilter::__construct(object $app)
 *
 * Constructs the AfterFilter object. The type of the $app object isn't
 * checked we only require that it provide an offsetGet method.
 */
static PHP_NAMED_FUNCTION(nr_laravel_afterfilter_construct) {
  zval* app = NULL;
  zval* this_obj = NULL;

  NR_UNUSED_RETURN_VALUE;
  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;

  if (SUCCESS
      == zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "o", &app)) {
    if (nr_php_object_has_method(app, "offsetGet" TSRMLS_CC)) {
      this_obj = NR_PHP_INTERNAL_FN_THIS();
      if (NULL == this_obj) {
        nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
        return;
      }

      /*
       * It's a valid app object. Set $this->app to contain it.
       */
      zend_update_property(nr_laravel_afterfilter_ce,
                           ZVAL_OR_ZEND_OBJECT(this_obj), NR_PSTR("app"),
                           app TSRMLS_CC);
    } else {
      /*
       * If this was userland code, we'd probably throw an exception here to
       * indicate that we can't really do anything, but it's easier if we're
       * silent here. On failure, we'll still produce a filter object that can
       * be installed; it just won't do anything because $this->app is null.
       */
      nrl_verbosedebug(
          NRL_FRAMEWORK, "%s: %.*s object doesn't have an offsetGet() method",
          __func__, NRSAFELEN(nr_php_class_entry_name_length(Z_OBJCE_P(app))),
          nr_php_class_entry_name(Z_OBJCE_P(app)));
    }
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: invalid parameters", __func__);
  }
}

/*
 * boolean AfterFilter::__invoke(object $request, object $response)
 *
 * This is called when the filter is fired, which is the appropriate time to
 * name the transaction.
 */
static PHP_NAMED_FUNCTION(nr_laravel_afterfilter_invoke) {
  zval* app = NULL;
  zval* router = NULL;
  zval* request = NULL;
  zval* response = NULL;
  zval* this_obj = NULL;

  NR_UNUSED_RETURN_VALUE_PTR;
  NR_UNUSED_RETURN_VALUE_USED;
  NR_UNUSED_THIS_PTR;

  /*
   * The return value is significant: it must be NULL, or later filters won't
   * be executed.
   */
  ZVAL_NULL(return_value);

  if (SUCCESS
      != zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET,
                                  ZEND_NUM_ARGS() TSRMLS_CC, "oo", &request,
                                  &response)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: invalid parameters", __func__);
    return;
  }

  this_obj = NR_PHP_USER_FN_THIS();
  if (NULL == this_obj) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot obtain 'this'", __func__);
    return;
  }

  /*
   * Check if $this->app is actually an object. If it's not, we won't attempt
   * to name the transaction.
   */
  app = nr_php_get_zval_object_property(this_obj, "app" TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(app)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: app property is not an object",
                     __func__);
    return;
  }

  /*
   * Get the router service from the container.
   */
  router = nr_php_call_offsetGet(app, "router" TSRMLS_CC);
  if (NULL == router) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get router service", __func__);
    return;
  }

  nr_laravel_name_transaction(router, request TSRMLS_CC);

  nr_php_zval_free(&router);
}

static zend_function_entry nr_laravel_afterfilter_functions[]
    = {ZEND_FENTRY(__construct,
                   nr_laravel_afterfilter_construct,
                   nr_laravel_afterfilter_construct_arginfo,
                   ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
           ZEND_FENTRY(__invoke,
                       nr_laravel_afterfilter_invoke,
                       nr_laravel_afterfilter_invoke_arginfo,
                       ZEND_ACC_PUBLIC) PHP_FE_END};

void nr_laravel_minit(TSRMLS_D) {
  zend_class_entry ce;

  INIT_CLASS_ENTRY(ce, "newrelic\\Laravel\\AfterFilter",
                   nr_laravel_afterfilter_functions);
  nr_laravel_afterfilter_ce = zend_register_internal_class(&ce TSRMLS_CC);
  zend_declare_property_null(nr_laravel_afterfilter_ce, NR_PSTR("app"),
                             ZEND_ACC_PRIVATE TSRMLS_CC);
}

static void nr_laravel_name_transaction_from_zval(const zval* name TSRMLS_DC) {
  char* name_terminated = nr_alloca(Z_STRLEN_P(name) + 1);

  nr_strxcpy(name_terminated, Z_STRVAL_P(name), Z_STRLEN_P(name));
  nr_txn_set_path("Laravel", NRPRG(txn), name_terminated, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);
}

static nr_status_t nr_laravel_name_transaction_from_route_action(
    const zval* action TSRMLS_DC) {
  nr_php_zval_unwrap(action);

  /*
   * In Laravel 4.0, the route action is a simple string. In later versions,
   * the action is an array: we want the "controller" element, which should be
   * a string.
   */
  if (nr_php_is_zval_valid_string(action)) {
    nrl_debug(NRL_FRAMEWORK,
              "%s: using Route::getAction() for transaction naming", __func__);
    nr_laravel_name_transaction_from_zval(action TSRMLS_CC);

    return NR_SUCCESS;
  } else if (nr_php_is_zval_valid_array(action)) {
    const zval* controller
        = nr_php_zend_hash_find(Z_ARRVAL_P(action), "controller");

    if (NULL == controller) {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "%s: no controller element in the action array",
                       __func__);
    } else if (!nr_php_is_zval_valid_string(controller)) {
      nrl_debug(NRL_FRAMEWORK,
                "%s: controller element in the action array is malformed",
                __func__);
    } else {
      nrl_debug(NRL_FRAMEWORK,
                "%s: using Route::getAction() for transaction naming",
                __func__);
      nr_laravel_name_transaction_from_zval(controller TSRMLS_CC);

      return NR_SUCCESS;
    }
  } else {
    nrl_debug(NRL_FRAMEWORK,
              "%s: unexpected type %d returned from Route::getAction()",
              __func__, Z_TYPE_P(action));
  }

  return NR_FAILURE;
}

/*
 * Convenience function: given a Route object and a method name, if that method
 * exists and returns a string, use that to name the transaction.
 */
static nr_status_t nr_laravel_name_transaction_from_route_method(
    zval* route,
    const char* method TSRMLS_DC) {
  if (nr_php_object_has_method(route, method TSRMLS_CC)) {
    zval* route_path_zv = NULL;

    route_path_zv = nr_php_call(route, method);
    if (nr_php_is_zval_valid_string(route_path_zv)) {
      nrl_debug(NRL_FRAMEWORK, "%s: using Route::%s() for transaction naming",
                __func__, method);
      nr_laravel_name_transaction_from_zval(route_path_zv TSRMLS_CC);

      nr_php_zval_free(&route_path_zv);
      return NR_SUCCESS;
    }
    nrl_verbosedebug(
        NRL_FRAMEWORK,
        "%s: Route::%s() returned an unexpected value/type, skipping. ",
        __func__, method);
    nr_php_zval_free(&route_path_zv);
  }

  return NR_FAILURE;
}

static void nr_laravel_name_transaction(zval* router, zval* request TSRMLS_DC) {
  zval* route = NULL;

  /*
   * We intentionally don't check if the router or request implement the
   * relevant interfaces. Unlike Symfony 2, Laravel mostly doesn't type hint
   * its internal method calls, which means that it's possible to replace these
   * services with something that exposes the same methods without implementing
   * the interfaces. As a result, we just check if they're an object and rely
   * on whether specific methods exist below.
   */
  if (!nr_php_is_zval_valid_object(router)) {
    nrl_debug(NRL_FRAMEWORK, "%s: router is not an object", __func__);
    return;
  }

  if (!nr_php_is_zval_valid_object(request)) {
    nrl_debug(NRL_FRAMEWORK, "%s: request is not an object", __func__);
    return;
  }

  /*
   * Most of the better names that are available are accessed through the Route
   * object, so let's grab that. Earlier versions of this code called
   * Router::currentRouteName() and Router::currentRouteAction(), which are
   * convenience methods that are less likely to be reimplemented in an
   * alternative implementation of the router than the current() or
   * getCurrentRoute() methods they depend upon.
   *
   * Laravel 4.1+ always provides current(), so we'll look for that first.
   * Laravel 4.0 used getCurrentRoute(), and some later versions of 4.2 have
   * re-added it as an alias for current() for improved backward compatibility,
   * which suggests that this is intended to be a stable public API.
   */
  if (nr_php_object_has_method(router, "current" TSRMLS_CC)) {
    route = nr_php_call(router, "current");
  } else if (nr_php_object_has_method(router, "getCurrentRoute" TSRMLS_CC)) {
    route = nr_php_call(router, "getCurrentRoute");
  } else {
    nrl_debug(
        NRL_FRAMEWORK,
        "%s: router does not provide a current() or getCurrentRoute() method",
        __func__);
  }

  if (nr_php_is_zval_valid_object(route)) {
    /*
     * If the Route object has a getName() method (added in Laravel 4.1.0),
     * then we'll prefer that over everything else.
     */
    if (nr_php_object_has_method(route, "getName" TSRMLS_CC)) {
      zval* route_name_zv = NULL;

      route_name_zv = nr_php_call(route, "getName");
      if (nr_php_is_zval_valid_string(route_name_zv)) {
        const char generated_prefix[] = "generated::";
        nr_string_len_t generated_prefix_len = sizeof(generated_prefix) - 1;

        if (nr_strncmp(generated_prefix, Z_STRVAL_P(route_name_zv),
                       generated_prefix_len)
            != 0) {
          char* route_name = nr_strndup(Z_STRVAL_P(route_name_zv),
                                        Z_STRLEN_P(route_name_zv));

          nrl_debug(NRL_FRAMEWORK,
                    "%s: using Route::getName() for transaction naming",
                    __func__);
          nr_txn_set_path("Laravel", NRPRG(txn), route_name,
                          NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);

          nr_php_zval_free(&route_name_zv);
          nr_free(route_name);
          goto leave;
        } else {
          nrl_verbosedebug(NRL_FRAMEWORK,
                           "%s: Route::getName() returned a randomly generated "
                           "route name, skipping. ",
                           __func__);
          nr_php_zval_free(&route_name_zv);
        }
      } else {
        nrl_verbosedebug(NRL_FRAMEWORK,
                         "%s: Route::getName() returned an unexpected "
                         "value/type, skipping. ",
                         __func__);
        nr_php_zval_free(&route_name_zv);
      }
    }

    /*
     * The next option is to get the action from Route::getAction().
     */
    if (nr_php_object_has_method(route, "getAction" TSRMLS_CC)) {
      zval* route_action = NULL;

      route_action = nr_php_call(route, "getAction");
      if (route_action) {
        nr_status_t named;

        named = nr_laravel_name_transaction_from_route_action(
            route_action TSRMLS_CC);
        nr_php_zval_free(&route_action);

        if (NR_SUCCESS == named) {
          goto leave;
        }
      }
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "%s: Route::getAction() returned an unexpected "
                       "value/type, skipping. ",
                       __func__);
    }

    /*
     * The next route-related option is to grab the route pattern from
     * Route::uri(), which is available in Laravel 4.1 to (at
     * least) 5.4, inclusive.
     */
    if (NR_SUCCESS
        == nr_laravel_name_transaction_from_route_method(route,
                                                         "uri" TSRMLS_CC)) {
      goto leave;
    }

    /*
     * To support Laravel 4.0 naming, the final route-related option is to grab
     * the route pattern from Route::getPath().
     */
    if (NR_SUCCESS
        == nr_laravel_name_transaction_from_route_method(route,
                                                         "getPath" TSRMLS_CC)) {
      goto leave;
    }
  } else if (route) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Route is an unexpected type: %d",
                     __func__, Z_TYPE_P(route));
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Route is null", __func__);
  }

  /*
   * We were unable to get the route. The final fallback is
   * to use "$METHOD/index.php". This is used instead of getting the URL from
   * the request object in order to reduce the chance of creating an MGI.
   */
  if (NR_SUCCESS
      == nr_laravel_should_assign_generic_path(NRPRG(txn), request TSRMLS_CC)) {
    zval* method_zv = NULL;

    method_zv = nr_php_call(request, "getMethod");
    if (nr_php_is_zval_valid_string(method_zv)) {
      char* name
          = nr_formatf("%.*s/index.php", NRSAFELEN(Z_STRLEN_P(method_zv)),
                       Z_STRVAL_P(method_zv));

      nrl_debug(NRL_FRAMEWORK,
                "%s: using Request::getMethod() fallback for transaction "
                "naming due to invalid Route object",
                __func__);
      nr_txn_set_path("Laravel", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                      NR_OK_TO_OVERWRITE);

      nr_php_zval_free(&method_zv);
      nr_free(name);
      goto leave;
    }

    nr_php_zval_free(&method_zv);
  }

  /*
   * Log the failure.
   */
  nrl_info(NRL_FRAMEWORK,
           "%s: unable to name Laravel transaction based on routing or request "
           "information",
           __func__);

leave:
  nr_php_zval_free(&route);
}

/*
 * We hook the application's exception handler to name transactions
 * when unhandled exceptions occur during request processing. Such
 * exceptions are caught by the framework's
 * Illuminate\Foundation\Http\Kernel::handle() method. In turn, a catch
 * block within handle() passes the exception to the exception handler's
 * render() method.
 *
 * See: http://laravel.com/docs/5.0/errors#handling-errors
 *
 */
NR_PHP_WRAPPER(nr_laravel5_exception_render) {
#if ZEND_MODULE_API_NO >= ZEND_5_4_X_API_NO
  const char* class_name = NULL;
  const char* ignored = NULL;
#else
  char* class_name = NULL;
  char* ignored = NULL;
#endif /* PHP >= 5.4 */

  char* name = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(NR_FW_LARAVEL, 5);

  /*
   * When the exception handler renders the response, name the transaction
   * after the exception handler using the same format used for controller
   * actions. e.g. Controller@action.
   */
  class_name = get_active_class_name(&ignored TSRMLS_CC);
  name = nr_formatf("%s@%s", class_name, get_active_function_name(TSRMLS_C));
  nr_txn_set_path("Laravel", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);
  nr_free(name);

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

/*
 * We hook the application's exception handler to report traced errors
 * for unhandled exceptions during request processing. Such exceptions
 * are caught by the framework's Illuminate\Foundation\Http\Kernel::handle()
 * method. In turn, a catch block within handle() passes the exception to
 * the exception handler's report() method.
 *
 * See: http://laravel.com/docs/5.0/errors#handling-errors
 */
NR_PHP_WRAPPER(nr_laravel5_exception_report) {
  zval* exception = NULL;
  int priority;
  zval* this_var = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(NR_FW_LARAVEL, 5);

  /*
   * PHP treats uncaught exceptions as E_ERROR, so we shall too.
   */
  priority = nr_php_error_get_priority(E_ERROR);
  if (NR_SUCCESS != nr_txn_record_error_worthy(NRPRG(txn), priority)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: not error worthy", __func__);
    NR_PHP_WRAPPER_LEAVE;
  }

  exception = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (NULL == exception) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: $e is NULL", __func__);
    goto leave;
  }

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  /*
   * Laravel 5's default exception handler is an instance of
   * Illuminate\Foundation\Exceptions\Handler, which includes a shouldReport()
   * method that returns false if the exception should be ignored.
   * Unfortunately, this isn't on the contract that Laravel exception handlers
   * are required to implement, but we'll see if the method exists, and if so,
   * we'll use that to determine whether we should record the exception.
   *
   * If the user has completely replaced the handler and hasn't implemented
   * this method, then we'll always report. Oversharing is likely better than
   * undersharing.
   */
  if (nr_php_object_has_method(this_var, "shouldReport" TSRMLS_CC)) {
    zval* retval;

    retval = nr_php_call(this_var, "shouldReport", exception);

    if (nr_php_is_zval_true(retval)) {
      nr_status_t st;

      st = nr_php_error_record_exception(NRPRG(txn), exception, priority,
                                         true /* add to segment */,
                                         NULL /* use default prefix */,
                                         &NRPRG(exception_filters) TSRMLS_CC);

      if (NR_FAILURE == st) {
        nrl_verbosedebug(NRL_FRAMEWORK, "%s: unable to record exception",
                         __func__);
      }
    } else {
      nrl_verbosedebug(
          NRL_FRAMEWORK,
          "%s: ignoring exception due to shouldReport returning false",
          __func__);
    }

    nr_php_zval_free(&retval);

  leave:
    NR_PHP_WRAPPER_CALL;
    nr_php_scope_release(&this_var);
    nr_php_arg_release(&exception);
  }
}
NR_PHP_WRAPPER_END

/*
 * Not applicable to OAPI.
 */
static void nr_laravel_register_after_filter(zval* app TSRMLS_DC) {
  zval* filter = NULL;
  zval* retval = NULL;
  zval* router = NULL;

  /*
   * We're going to call Router::after() to register a filter for
   * transaction naming. Unfortunately, after() filters don't get the
   * Application object as one of their parameters, so we use the AfterFilter
   * object that is declared elsewhere in this file to emulate a closure that
   * captures the Application object. (The Zend Engine API is insufficient
   * to use a true closure.)
   */

  /*
   * Get the router service from the container.
   */
  router = nr_php_call_offsetGet(app, "router" TSRMLS_CC);
  if (NULL == router) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get router service", __func__);
    return;
  }

  /*
   * Only install our filter if this version of Laravel supports them.
   * Filters were deprecated in Laravel 5.0 and removed in version 5.2.
   * As such, not applicable to OAPI.
   */
  if (0 == nr_php_object_has_concrete_method(router, "after" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Router does not support filters",
                     __func__);
    goto end;
  }

  filter = nr_php_zval_alloc();
  object_init_ex(filter, nr_laravel_afterfilter_ce);

  retval = nr_php_call(filter, "__construct", app);
  if (NULL == retval) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: error constructing AfterFilter object",
                     __func__);
    goto end;
  }
  nr_php_zval_free(&retval);

  retval = nr_php_call(router, "after", filter);
  if (NULL == retval) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: error installing AfterFilter",
                     __func__);
    goto end;
  }

end:
  nr_php_zval_free(&router);
  nr_php_zval_free(&filter);
  nr_php_zval_free(&retval);
}
/*
 * Not applicable to OAPI.
 */
NR_PHP_WRAPPER(nr_laravel4_application_run) {
  zval* this_var = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(NR_FW_LARAVEL, 4);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  if (0 == nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application object is invalid",
                     __func__);
    goto leave;
  }

  nr_laravel_register_after_filter(this_var TSRMLS_CC);

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Purpose : Wrap implementations of the Middleware interface, and update
 *           the transaction name. This ensures the transaction is named if
 *           the middleware short-circuits request processing by returning
 *           a response instead of invoking its successor.
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility. This entails that the last wrapped call gets to name the
 * txn (as detailed in the purpose above).
 */
NR_PHP_WRAPPER(nr_laravel5_middleware_handle) {
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(NR_FW_LARAVEL, 5);

  if (wraprec->classname) {
    char* name = NULL;

    name = nr_formatf("%s::%s", wraprec->classname, wraprec->funcname);
    nr_txn_set_path("Laravel", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
    nr_free(name);
  } else {
    nr_txn_set_path("Laravel", NRPRG(txn), wraprec->funcname,
                    NR_PATH_TYPE_ACTION, NR_OK_TO_OVERWRITE);
  }

  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

static void nr_laravel5_wrap_middleware(zval* app TSRMLS_DC) {
  zval* kernel = NULL;
  zval* middleware = NULL;
  zval* classname = NULL;

  kernel = nr_php_call_offsetGet(
      app, "Illuminate\\Contracts\\Http\\Kernel" TSRMLS_CC);

  if (0 == nr_php_is_zval_valid_object(kernel)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get HTTP kernel", __func__);
    goto leave;
  }

  /*
   * Wrap each global middleware so the transaction will be named
   * after the last middleware to execute in the event one of them
   * terminates request processing.
   */
  middleware = nr_php_get_zval_object_property(kernel, "middleware" TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_array(middleware)) {
    if (NULL == middleware) {
      nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get HTTP middleware",
                       __func__);
      goto leave;
    }

    if (nr_php_is_zval_valid_object(middleware)) {
      nrl_verbosedebug(
          NRL_FRAMEWORK, "%s: HTTP middleware is an unexpected object: %*s.",
          __func__,
          NRSAFELEN(nr_php_class_entry_name_length(Z_OBJCE_P(middleware))),
          nr_php_class_entry_name(Z_OBJCE_P(middleware)));
    } else {
      nrl_verbosedebug(NRL_FRAMEWORK,
                       "%s: HTTP middleware is an unexpected type: %d",
                       __func__, Z_TYPE_P(middleware));
    }

    goto leave;
  }

  ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(middleware), classname) {
    if (nr_php_is_zval_valid_string(classname)) {
      char* name = NULL;

      name = nr_formatf("%.*s::handle", (int)Z_STRLEN_P(classname),
                        Z_STRVAL_P(classname));
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
      nr_php_wrap_user_function_before_after(
          name, nr_strlen(name), nr_laravel5_middleware_handle, NULL);
#else
      nr_php_wrap_user_function(name, nr_strlen(name),
                                nr_laravel5_middleware_handle TSRMLS_CC);
#endif
      nr_free(name);
    }
  }
  ZEND_HASH_FOREACH_END();

leave:
  nr_php_zval_free(&kernel);
}

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
 *           4. The post callback.
 *
 * Note: In this case, all functions utilized this execute before calling
 * `NR_PHP_WRAPPER_CALL` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility.
 */
static void nr_laravel_add_callback_method(const zend_class_entry* ce,
                                           const char* method,
                                           size_t method_len,
                                           nrspecialfn_t callback TSRMLS_DC) {
  const char* class_name = NULL;
  size_t class_name_len;
  zend_function* function = NULL;

  if (NULL == ce) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: class entry is NULL", __func__);
    return;
  }

  class_name = nr_php_class_entry_name(ce);
  class_name_len = nr_php_class_entry_name_length(ce);

  function = nr_php_find_class_method(ce, method);
  if (NULL == function) {
    nrl_verbosedebug(NRL_FRAMEWORK, "cannot get function entry for %.*s::%.*s",
                     NRSAFELEN(class_name_len), class_name,
                     NRSAFELEN(method_len), method);
    return;
  }

  char* class_method = nr_formatf("%.*s::%.*s", NRSAFELEN(class_name_len),
                                  class_name, NRSAFELEN(method_len), method);

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      class_method, nr_strlen(class_method), callback, NULL);
#else
  nr_php_wrap_user_function(class_method, nr_strlen(class_method),
                            callback TSRMLS_CC);
#endif
  nr_free(class_method);
}

NR_PHP_WRAPPER(nr_laravel5_application_boot) {
  zval* this_var = NULL;
  zval* exception_handler = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK_VERSION(NR_FW_LARAVEL, 5);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: Application object is invalid",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  nr_laravel_register_after_filter(this_var TSRMLS_CC);
  nr_laravel5_wrap_middleware(this_var TSRMLS_CC);

  /*
   * Laravel 5 has a known interface applications can implement to supplement or
   * replace the default error handling. This is convenient because it allows us
   * to sensibly name transactions when an exception is thrown during routing
   * and also to record the error.
   */
  exception_handler = nr_php_call_offsetGet(
      this_var, "Illuminate\\Contracts\\Debug\\ExceptionHandler" TSRMLS_CC);
  if (nr_php_is_zval_valid_object(exception_handler)) {
    nr_laravel_add_callback_method(Z_OBJCE_P(exception_handler),
                                   NR_PSTR("render"),
                                   nr_laravel5_exception_render TSRMLS_CC);

    nr_laravel_add_callback_method(Z_OBJCE_P(exception_handler),
                                   NR_PSTR("report"),
                                   nr_laravel5_exception_report TSRMLS_CC);
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s: cannot get exception handler",
                     __func__);
  }

end:
  nr_php_zval_free(&exception_handler);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * This is a generic callback for any post hook on an Illuminate\Routing\Router
 * method where the method receives a request object as its first parameter.
 *
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to the OAPI default of calling
 * the wrapped function callback in func_end, there are no changes required to
 * ensure OAPI compatibility. This entails that the first call to this function
 * gets to name the txn.
 */
NR_PHP_WRAPPER(nr_laravel_router_method_with_request) {
  zval* request = NULL;
  zval* router_object = NULL;
  zval* router_app_key = NULL;

  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);

  /*
   * Laravel 5.5 turned prepareResponse into a static method.  So, if we're
   * here, and the current function is a static function, we'll use Laravel's
   * userland `app` function to grab an instance of the main router object
   * instead
   */
  zend_function* func = NULL;
  func = nr_php_execute_function(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (nr_php_function_is_static_method(func)) {
    router_app_key = nr_php_zval_alloc();
    nr_php_zval_str(router_app_key, "router");
    router_object = nr_php_call(NULL, "app", router_app_key);
  } else {
    router_object = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  }

  request = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_laravel_name_transaction(router_object, request TSRMLS_CC);

  nr_php_arg_release(&request);
  nr_php_zval_free(&router_object);
  nr_php_zval_free(&router_app_key);
}
NR_PHP_WRAPPER_END

/*
 * Illuminate\Foundation\Application::__construct() is the earliest chance
 * to detect the Laravel version and apply the corresponding instrumentation.
 * The version number is only available via the Application::VERSION constant,
 * which we cannot access until after the class has been parsed.
 */
NR_PHP_WRAPPER(nr_laravel_application_construct) {
  zval* this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  ;
  char* version = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  /*
   * Retrieving the version is not included in the INI check below because it is
   * needed for Laravel's instrumentation.
   */
  version = nr_php_get_object_constant(this_var, "VERSION");

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    // Add php package to transaction
    nr_txn_add_php_package(NRPRG(txn), "laravel/framework", version);
  }

  if (version) {
    nrl_debug(NRL_FRAMEWORK, "Laravel version is " NRP_FMT, NRP_PHP(version));

    if (php_version_compare(version, "5.0") < 0) {
      NRPRG(framework_version) = 4;

      /* Laravel 4.x */
      nr_php_wrap_user_function(
          NR_PSTR("Illuminate\\Foundation\\Application::run"),
          nr_laravel4_application_run TSRMLS_CC);

      if (php_version_compare(version, "4.1") < 0) {
        /* Laravel 4.0 */
        nr_php_wrap_user_function(
            NR_PSTR("Illuminate\\Routing\\Router::callAfterFilter"),
            nr_laravel_router_method_with_request TSRMLS_CC);
      }
    } else {
      NRPRG(framework_version) = 5;

      /* Laravel >= 5.0 */
      nr_php_wrap_user_function(
          NR_PSTR("Illuminate\\Foundation\\Application::boot"),
          nr_laravel5_application_boot TSRMLS_CC);
    }

    nr_free(version);
  }

  /*
   * If router filtering is disabled, then the filter installed by the previous
   * callback will never fire. These callbacks attempt to mitigate that, but
   * won't cover the (currently unsupported) case where the router service has
   * been replaced and the normal Illuminate\Routing\Router methods aren't
   * called.
   *
   * If router filtering is enabled, then we may set the transaction name
   * multiple times. This isn't considered to be an issue, as the last one will
   * win, and that's almost certain to be the correct one. If this turns out to
   * cause more performance overhead than we're comfortable with, then the
   * simple fix would be to check if filtering is enabled in
   * nr_laravel_router_method_with_request.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Illuminate\\Routing\\Router::prepareResponse"),
      nr_laravel_router_method_with_request TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * * txn naming scheme:
 * In this case, `nr_txn_set_path` is called before `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_begin it needs to be explicitly set as a before_callback to ensure
 * OAPI compatibility.
 * This entails that the last wrapped call gets to name the txn.
 */
NR_PHP_WRAPPER(nr_laravel_console_application_dorun) {
  zval* command = NULL;
  zval* input = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LARAVEL);

  /*
   * The first parameter to this method should be an instance of an
   * InputInterface, which defines a method called getFirstArgument which will
   * return the command name, or an empty string if no command name was given.
   * We can then use that with an appropriate prefix to name the transaction.
   */
  input = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_object_instanceof_class(
          input,
          "Symfony\\Component\\Console\\Input\\InputInterface" TSRMLS_CC)) {
    goto leave;
  }

  command = nr_php_call(input, "getFirstArgument");
  if (nr_php_is_zval_non_empty_string(command)) {
    char* name = nr_formatf("Artisan/%.*s", NRSAFELEN(Z_STRLEN_P(command)),
                            Z_STRVAL_P(command));

    nr_txn_set_path("Laravel", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);

    nr_free(name);
  } else {
    /*
     * Not having any arguments will result in the same behaviour as
     * "artisan list", so we'll name the transaction accordingly.
     */
    nr_txn_set_path("Laravel", NRPRG(txn), "Artisan/list", NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }

leave:
  NR_PHP_WRAPPER_CALL;
  nr_php_arg_release(&input);
  nr_php_zval_free(&command);
}
NR_PHP_WRAPPER_END

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds the OAPI default of calling the
 * wrapped function callback in func_end, there are no changes required to
 * ensure OAPI compatibility. This entails that the first call to this function
 * gets to name the txn.
 */
NR_PHP_WRAPPER(nr_laravel_routes_get_route_for_methods) {
  zval* arg_request = NULL;
  zval* http_method = NULL;
  zval* new_name = NULL;
  zval* route_name = NULL;
  zval** route = NULL;

  NR_UNUSED_SPECIALFN;
  (void)wraprec;

  /*
   * Start by calling the original method, and if it doesn't return a
   * route then we don't need to do any extra work.
   */
  route = NR_GET_RETURN_VALUE_PTR;
  NR_PHP_WRAPPER_CALL;

  /* If the method did not return a route, then end gracefully. */
  if (NULL == route || !nr_php_is_zval_valid_object(*route)) {
    goto end;
  }

  /* Grab the first argument, which should be a request. */
  arg_request = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (!nr_php_is_zval_valid_object(arg_request)) {
    goto end;
  }

  /*
   * Call the ->method() method on the request and marshall the
   * string to something we can compare.
   */
  http_method = nr_php_call(arg_request, "method");
  if (NULL == http_method || !nr_php_is_zval_valid_string(http_method)) {
    goto end;
  }

  /* Now that we have a response from ->method, check if this is an HTTP OPTIONS
   * request and gracefully end if it isn't.
   */
  if (0
      != nr_strnicmp("OPTIONS", Z_STRVAL_P(http_method),
                     Z_STRLEN_P(http_method))) {
    goto end;
  }

  /*
   * If the route name is NOT a null php value, that means some future Laravel
   * version or user customizations has started naming these CORS HTTP OPTIONS
   * requests. This means there's no risk and we should respect their naming.
   */
  route_name = nr_php_call(*route, "getName");
  if (!nr_php_is_zval_null(route_name)) {
    goto end;
  }

  /* this is an CORS HTTP OPTIONS request
   * that will generate an MGI unless we do something to name the transaction.
   * To prevent the MGI, we name the route _CORS_OPTIONS, which will result in
   * a transaction with the same name.
   */
  new_name = nr_php_zval_alloc();
  nr_php_zval_str(new_name, "_CORS_OPTIONS");

  nr_php_call(*route, "name", new_name);

end:
  nr_php_arg_release(&arg_request);
  nr_php_zval_free(&http_method);
  nr_php_zval_free(&new_name);
  nr_php_zval_free(&route_name);
}
NR_PHP_WRAPPER_END

static nr_status_t nr_laravel_should_assign_generic_path(const nrtxn_t* txn,
                                                         zval* request
                                                             TSRMLS_DC) {
  /*
   * If the request object doesn't have a getMethod method then
   * exit gracefully.
   */
  if (!nr_php_object_has_method(request, "getMethod" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "%s: Request object has no getMethod method. Bailing.",
                     __func__);
    return NR_FAILURE;
  }

  /*
   * If the transaction has a path name that is "unknown", its always better
   * to replace it with the generic path.
   */
  if (nr_txn_is_current_path_named(txn, "unknown")) {
    return NR_SUCCESS;
  }

  /*
   * If the transaction has a name other than "unknown", but its path_type is
   * less than the NR_PATH_TYPE_ACTION set in `nr_laravel_enable` (i.e. the name
   * is coming from outside this library), then we should assign the
   * $METHOD/index.php name.
   */
  if (txn->status.path_type < NR_PATH_TYPE_ACTION) {
    return NR_SUCCESS;
  }

  nrl_verbosedebug(NRL_FRAMEWORK,
                   "%s: No condition met, so will not assign generic laravel "
                   "path. path=%s, path_type=%i",
                   __func__, txn->path, (int)txn->status.path_type);
  return NR_FAILURE;
}

void nr_laravel_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent having to name routing errors.
   * This follows what is done in symfony2.
   */
  nr_txn_set_path("Laravel", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  /*
   * This is tricky: we want to install a callback using Application::after(),
   * but we want to do it after services have been set up (and overridden, if
   * the user is replacing one or more services), since Application::after() is
   * dependent on having the right router service available. The best place to
   * do so depends on the version of Laravel. Wait until the Application class
   * has been loaded to install the callback.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Illuminate\\Foundation\\Application::__construct"),
      nr_laravel_application_construct TSRMLS_CC);

  /**
   * The getRouteForMethods method can end up generating a laravel route
   * for an OPTIONS request on a URL that has a handler for another HTTP
   * verb.  We need to detect this condition and generate a reasonable
   * name for these OPTIONS routes, as the default naming will often
   * end up creating an MGI.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Illuminate\\Routing\\RouteCollection::getRouteForMethods"),
      nr_laravel_routes_get_route_for_methods TSRMLS_CC);
  /*
   * Listen for Artisan commands so we can name those appropriately.
   */
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO \
    && !defined OVERWRITE_ZEND_EXECUTE_DATA
  nr_php_wrap_user_function_before_after(
      NR_PSTR("Illuminate\\Console\\Application::doRun"),
      nr_laravel_console_application_dorun, NULL);
#else
  nr_php_wrap_user_function(NR_PSTR("Illuminate\\Console\\Application::doRun"),
                            nr_laravel_console_application_dorun TSRMLS_CC);
#endif
  /*
   * Start Laravel queue instrumentation, provided it's not disabled.
   */
  if (0 == NR_PHP_PROCESS_GLOBALS(special_flags).disable_laravel_queue) {
    nr_laravel_queue_enable(TSRMLS_C);
  }
}
