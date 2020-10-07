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

/* Determine whether a Kohana request or route is actually an external call. */
static int nr_kohana_is_external_request(zval* request TSRMLS_DC) {
  int is_external = 0;

  if (nr_php_object_has_method(request, "is_external" TSRMLS_CC)) {
    zval* retval;

    retval = nr_php_call(request, "is_external");
    is_external = nr_php_is_zval_true(retval);
    nr_php_zval_free(&retval);
  }

  return is_external;
}

/*
 * We trap calls to Kohana_Request::execute. We then verify two
 * preconditions. 1) The request is internal (i.e. incoming to the app),
 * and 2) the request matched a defined route. If both conditions are
 * met, we name the transaction 'Controller/Action' where the values
 * are retrieved from the request object. Note, the controller and action
 * are only valid if a route was found.
 */
NR_PHP_WRAPPER(nr_kohana_name_the_wt) {
  zval* this_var = NULL;
  zval* route = NULL;

  (void)wraprec;
  NR_UNUSED_SPECIALFN;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_KOHANA);

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  if (!nr_php_is_zval_valid_object(this_var)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "Kohana: invalid object");
    goto leave;
  }

  if (nr_kohana_is_external_request(this_var TSRMLS_CC)) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Kohana: request is external, no name this time");
    goto leave;
  }

  if ((0 == nr_php_object_has_method(this_var, "route" TSRMLS_CC))
      || (0 == nr_php_object_has_method(this_var, "controller" TSRMLS_CC))
      || (0 == nr_php_object_has_method(this_var, "action" TSRMLS_CC))) {
    nrl_verbosedebug(NRL_FRAMEWORK,
                     "Kohana: object inconsistent with a Kohana_Request");
    goto leave;
  }

  route = nr_php_call(this_var, "route");
  if (nr_php_is_zval_valid_object(route)) {
    /* Found a route, should have a valid controller and action. */
    zval* controller = NULL;
    zval* action = NULL;
    char name[255 + 1]; /* max metric name length + 1 */

    controller = nr_php_call(this_var, "controller");
    action = nr_php_call(this_var, "action");

    name[0] = '\0';
    snprintf(
        name, sizeof(name), "%.*s/%.*s",
        nr_php_is_zval_non_empty_string(controller)
            ? NRSAFELEN(Z_STRLEN_P(controller))
            : 32,
        nr_php_is_zval_non_empty_string(controller) ? Z_STRVAL_P(controller)
                                                    : "NoController",
        nr_php_is_zval_non_empty_string(action) ? NRSAFELEN(Z_STRLEN_P(action))
                                                : 32,
        nr_php_is_zval_non_empty_string(action) ? Z_STRVAL_P(action)
                                                : "NoAction");
    nr_php_zval_free(&controller);
    nr_php_zval_free(&action);
    nr_txn_set_path("Kohana", NRPRG(txn), name, NR_PATH_TYPE_ACTION,
                    NR_OK_TO_OVERWRITE);
  }

leave:
  nr_php_zval_free(&route);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

/*
 * Enable the Kohana instrumentation
 */
void nr_kohana_enable(TSRMLS_D) {
  /* We set the path to 'unknown' to prevent having to name routing errors. */
  nr_txn_set_path("Kohana", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);
  nr_php_wrap_user_function(NR_PSTR("Kohana_Request::execute"),
                            nr_kohana_name_the_wt TSRMLS_CC);
}
