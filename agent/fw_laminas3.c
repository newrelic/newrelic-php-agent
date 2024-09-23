/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "util_memory.h"

#define PHP_PACKAGE_NAME "laminas/laminas-mvc"

/*
 * Laminas is a rebranding of Zend, but the logic remains the same,
 * it is simply a name change and corresponds directly to Zend 3.x.
 * Compare to `fw_zend2.c`
 *
 * How Laminas nee Zend Routing Works
 * =====================
 * Laminas (it starts at version 3) has a Laminas\\Router
 * that decides which controller to call.
 *
 * Config is done in `module/Application/config/module.config.php`
 * (which exists per-module), which is a
 * PHP file returning an associative array containing something that looks like
 * this example from the Laminas Skeleton App tutorial
 * https://docs.laminas.dev/tutorials/getting-started/routing-and-controllers/.
 *
 *   'router' => [
 *        'routes' => [
 *            'album' => [
 *                'type'    => Segment::class,
 *                'options' => [
 *                    'route' => '/album[/:action[/:id]]',
 *                   'constraints' => [
 *                        'action' => '[a-zA-Z][a-zA-Z0-9_-]*',
 *                        'id'     => '[0-9]+',
 *                    ],
 *                    'defaults' => [
 *                        'controller' => Controller\AlbumController::class,
 *                        'action'     => 'index',
 *                    ],
 *                ],
 *            ],
 *        ],
 *    ],
 *
 *    ...
 *
 * Here, 'album' is the name of a route, and maps to some controller there is
 * an onRoute event that corresponds to making routing happen. We would
 * probably like to have some instrumentation of the type of actions that a
 * controller executes if the action is something like 'view' or 'list' or
 * 'edit', but 'id' is likely to be sensitive, so all we get is the route name.
 *
 * One approach would be to instrument the onRoute event; we ended up going
 * with the setMatchedRouteName instead and just setting the path whenever that
 * gets called (which seems to be once per request).
 */

/*
 * The first approach had been to use EG (return_value_ptr_ptr), but that came
 * back null. All three versions of the instrumented function return $this, so
 * presumably that was some optimization due to the return value not being used.
 */

/*
 * txn naming scheme:
 * In this case, `nr_txn_set_path` is called after `NR_PHP_WRAPPER_CALL` with
 * `NR_OK_TO_OVERWRITE` and as this corresponds to calling the wrapped function
 * in func_end no change is needed to ensure OAPI compatibility as it will use
 * the default func_end after callback. This entails that the first wrapped
 * function call of this type gets to name the txn.
 */
NR_PHP_WRAPPER(nr_laminas3_name_the_wt) {
  zval* path = NULL;
  zval* this_var = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_LAMINAS3);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0
      == nr_php_object_has_method(this_var, "getMatchedRouteName" TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: this_var doesn't have getMatchedRouteName.",
                     __func__);
    NR_PHP_WRAPPER_CALL;
    goto leave;
  }

  NR_PHP_WRAPPER_CALL;

  path = nr_php_call(this_var, "getMatchedRouteName");

  if (nr_php_is_zval_valid_string(path)) {
    char* path_term = nr_strndup(Z_STRVAL_P(path), Z_STRLEN_P(path));

    nr_txn_set_path("Laminas3", NRPRG(txn), path_term, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: this_var has getMatchedRouteName = %s.", __func__,
                     path_term);
    nr_free(path_term);
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: couldn't getMatchedRouteName on setter hook.",
                     __func__);
  }

  nr_php_zval_free(&path);

leave:
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_laminas3_enable(TSRMLS_D) {
  nr_txn_set_path("Laminas3", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_OK_TO_OVERWRITE);

  /*
   * We instrument all three of these. The Console one is used for
   * Laminas3 Console requests
   * https://docs.laminas.dev/laminas-console/intro/.
   * In Laminas3, HTTP and Console both inherit from the third,
   * so it's unlikely that that method will be called unless someone is
   * using custom routing.
   */
  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Mvc\\Router\\HTTP\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Mvc\\Router\\Console\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Mvc\\Router\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);

  /*
   * The functions above were moved to a new package and namespace in
   * version 3.0.
   * https://github.com/laminas/laminas-mvc-console/blob/master/docs/book/migration/v2-to-v3.md
   *
   * The namespace Laminas\Mvc\Router\Console becomes Laminas\Mvc\Console\Router
   *
   * https://github.com/laminas/laminas-router
   */

  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Router\\HTTP\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Router\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);
  nr_php_wrap_user_function(
      NR_PSTR("Laminas\\Mvc\\Console\\Router\\RouteMatch::setMatchedRouteName"),
      nr_laminas3_name_the_wt TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
    nr_txn_php_package_set_options(NRPRG(txn), PHP_PACKAGE_NAME,
                                   NR_PHP_PACKAGE_OPTION_MAJOR_METRIC);
  }
}
