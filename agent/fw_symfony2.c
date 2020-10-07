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
#include "fw_symfony_common.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

NR_PHP_WRAPPER(nr_symfony2_name_the_wt) {
  zval* event = NULL;
  zval* request = NULL;

  /* Warning avoidance. */
  (void)wraprec;

  NR_PHP_WRAPPER_REQUIRE_FRAMEWORK(NR_FW_SYMFONY2);

  /*
   * A high level overview of the logic:
   *
   * RouterListener::onKernelRequest() receives a GetResponseEvent parameter,
   * which includes the request object accessible via the getRequest() method.
   * We want to get the request, then access its attributes: the request
   * matcher will create a number of internal attributes prefixed by
   * underscores as part of resolving the controller action.
   *
   * If the user has given their action method a friendly name via an
   * annotation or controller option, then this is available in _route. This is
   * likely to be shorter and clearer than the auto-generated controller
   * method, so it's the first preference.
   *
   * If _route doesn't exist, then _controller should always exist. For
   * non-subrequests, this will be a name Symfony generates from the fully
   * qualified class name and method. For subrequests, this is whatever the
   * user gave Controller::forward(), which will hopefully be more or less the
   * same thing.
   */

  event = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(event)) {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 2: RouterListener::onKernelRequest() does not "
                     "have an event parameter");
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  NR_PHP_WRAPPER_CALL;

  /* Get the request object from the event. */
  request = nr_php_call(event, "getRequest");
  if (nr_php_object_instanceof_class(
          request, "Symfony\\Component\\HttpFoundation\\Request" TSRMLS_CC)) {
    /* Look for _route first. */
    zval* route_rval
        = nr_symfony_object_get_string(request, "_route" TSRMLS_CC);

    if (route_rval) {
      if (NR_SUCCESS
          != nr_symfony_name_the_wt_from_zval(route_rval TSRMLS_CC,
                                              "Symfony 2")) {
        nrl_verbosedebug(
            NRL_TXN, "Symfony 2: Request::get('_route') returned a non-string");
      }
      nr_php_zval_free(&route_rval);
    } else {
      /* No _route. Look for _controller. */
      zval* controller_rval
          = nr_symfony_object_get_string(request, "_controller" TSRMLS_CC);

      if (controller_rval) {
        if (NR_SUCCESS
            != nr_symfony_name_the_wt_from_zval(controller_rval TSRMLS_CC,
                                                "Symfony 2")) {
          nrl_verbosedebug(
              NRL_TXN,
              "Symfony 2: Request::get('_controller') returned a non-string");
        }
        nr_php_zval_free(&controller_rval);
      } else {
        nrl_verbosedebug(NRL_TXN,
                         "Symfony 2: Neither _controller nor _route is set");
      }
    }
  } else {
    nrl_verbosedebug(NRL_TXN,
                     "Symfony 2: GetResponseEvent::getRequest() returned a "
                     "non-Request object");
  }

end:
  nr_php_arg_release(&event);
  nr_php_zval_free(&request);
}
NR_PHP_WRAPPER_END

void nr_symfony2_enable(TSRMLS_D) {
  /*
   * We set the path to 'unknown' to prevent having to name routing errors.
   */
  nr_txn_set_path("Symfony2", NRPRG(txn), "unknown", NR_PATH_TYPE_ACTION,
                  NR_NOT_OK_TO_OVERWRITE);

  /*
   * Originally, we had a pre-callback hook on HttpKernel::filterResponse().
   * This works fine for simple requests, but fails on subrequests forwarded by
   * Controller::forward() due to HttpKernel::filterResponse() being called in
   * the reverse order as Symfony unwinds the request stack, which means we get
   * the initial request name rather than the innermost, which is what we want.
   *
   * We'll hook into the RouterListener. Once onKernelRequest() has
   * finished its work, the controller has been resolved, so we can go from
   * there. This is reliable as long as the user hasn't replaced the router
   * listener service, which is a pretty deep customisation: chances are a user
   * who's doing that is quite capable of naming a transaction by hand.
   */
  nr_php_wrap_user_function(NR_PSTR("Symfony\\Component\\HttpKernel\\EventListe"
                                    "ner\\RouterListener::onKernelRequest"),
                            nr_symfony2_name_the_wt TSRMLS_CC);
}
