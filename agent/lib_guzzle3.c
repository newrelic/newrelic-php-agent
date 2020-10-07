/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Guzzle is a general purpose library for making HTTP requests. It supports
 * asynchronous, parallel requests using curl_multi_exec() while providing a
 * modern OO API for users.
 *
 * It is a required component in Drupal 8, and strongly recommended by other
 * frameworks, including Symfony 2.
 *
 * The general approach used is to watch for calls to
 * Guzzle\Http\Message\Request::setState(): if the state is changing to
 * STATE_TRANSFER or STATE_COMPLETE, then we know a request is about to be
 * issued or has just completed, respectively.
 *
 * Source : https://github.com/guzzle/guzzle
 * Docs   : https://guzzle.readthedocs.org/en/latest/
 */

#include "php_agent.h"
#include "php_call.h"
#include "php_user_instrument.h"
#include "php_execute.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "lib_guzzle_common.h"
#include "nr_header.h"
#include "nr_segment_external.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_strings.h"

static int nr_guzzle3_in_redirect_iterator(zval* frame,
                                           int* in_guzzle_ptr,
                                           zend_hash_key* key NRUNUSED
                                               TSRMLS_DC) {
  int idx;
  const zval* klass;

  NR_UNUSED_TSRMLS;

  if (0 == nr_php_is_zval_valid_array(frame)) {
    return ZEND_HASH_APPLY_KEEP;
  }
  if (NULL == in_guzzle_ptr) {
    return ZEND_HASH_APPLY_KEEP;
  }

  klass = nr_php_zend_hash_find(Z_ARRVAL_P(frame), "class");

  if (0 == nr_php_is_zval_non_empty_string(klass)) {
    return ZEND_HASH_APPLY_KEEP;
  }

  /*
   * NOTE: RedirectPlugin was added in Guzzle version v3.0.3
   * and therefore this approach will only work on that version or later.
   */
  idx = nr_strncaseidx(Z_STRVAL_P(klass), "RedirectPlugin", Z_STRLEN_P(klass));
  if (idx >= 0) {
    *in_guzzle_ptr = 1;
  }

  return ZEND_HASH_APPLY_KEEP;
}

static int nr_guzzle3_in_redirect(TSRMLS_D) {
  int in_redirect = 0;
  zval* stack = nr_php_backtrace(TSRMLS_C);

  if (nr_php_is_zval_valid_array(stack)) {
    nr_php_zend_hash_zval_apply(
        Z_ARRVAL_P(stack), (nr_php_zval_apply_t)nr_guzzle3_in_redirect_iterator,
        &in_redirect TSRMLS_CC);
  }

  nr_php_zval_free(&stack);

  return in_redirect;
}

/*
 * Purpose : Checks if the given state matches the expected state.
 *
 * Params  : 1. The expected state constant, as a string.
 *           2. The actual state zval.
 *           3. The request object from which we want to retrieve the expected
 *              constant value.
 *
 * Returns : Non-zero if the state matches, zero if it doesn't.
 */
static int nr_guzzle3_is_state(const char* expected,
                               zval* state,
                               zval* request TSRMLS_DC) {
  zval* expected_const = NULL;
  int is_complete = 0;
  zval result;

  if ((NULL == state) || (0 == nr_php_is_zval_valid_object(request))) {
    return 0;
  }

  /* Get the value of the expected state constant. */
  expected_const = nr_php_get_class_constant(Z_OBJCE_P(request), expected);
  if (NULL == expected_const) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle 3: Request class does not have a %s constant",
                     expected);
    return 0;
  }

  /* See if the constant and the state are identical. */
  nr_php_zval_bool(&result, 0);
  if (SUCCESS
      == is_identical_function(&result, expected_const, state TSRMLS_CC)) {
    is_complete = nr_php_is_zval_true(&result);
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle 3: is_identical_function failed when checking the "
                     "request state");
  }

  nr_php_zval_free(&expected_const);
  return is_complete;
}

/*
 * Purpose : Returns an item from the cURL transfer information stored within a
 *           Guzzle Response object.
 *
 * Params  : 1. The key of the item to return.
 *           2. The response object.
 *
 * Returns : The zval returned by Response::getInfo(). This needs to be
 *           destroyed with nr_php_zval_free() when no longer needed.
 */
static zval* nr_guzzle3_response_get_info(const char* key,
                                          zval* response TSRMLS_DC) {
  zval* param = nr_php_zval_alloc();
  zval* retval = NULL;

  nr_php_zval_str(param, key);

  retval = nr_php_call(response, "getInfo", param);
  if (NULL == retval) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle 3: error calling Response::getInfo('" NRP_FMT "')",
                     NRP_ARGUMENTS(key));
  }

  nr_php_zval_free(&param);
  return retval;
}

/*
 * Purpose : Handles a request transitioning into the STATE_TRANSFER state.
 *
 * Params  : 1. The request object.
 */
static void nr_guzzle3_request_state_transfer(zval* request TSRMLS_DC) {
  nr_segment_t* segment;

  /*
   * Add the request object to those we're tracking.
   */
  segment = nr_guzzle_obj_add(request, "Guzzle 3" TSRMLS_CC);

  /*
   * Set the request headers.
   */
  nr_guzzle_request_set_outbound_headers(request, segment TSRMLS_CC);
}

/*
 * Purpose : Handles a request transitioning into the STATE_COMPLETE state.
 *
 * Params  : 1. The request object.
 */
static void nr_guzzle3_request_state_complete(zval* request TSRMLS_DC) {
  nrtime_t duration;
  nr_segment_t* segment;
  nr_segment_external_params_t external_params = {.library = "Guzzle 3"};
  zval* response = NULL;
  zval* time = NULL;
  zval* status = NULL;
  zval* url = NULL;

  if (NR_FAILURE
      == nr_guzzle_obj_find_and_remove(request, &segment TSRMLS_CC)) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Guzzle 3: Request object entered STATE_COMPLETE without "
                     "being tracked");
    return;
  }

  /*
   * We can get the total request time by calling getInfo('total_time') on the
   * response object.
   */
  response = nr_php_call(request, "getResponse");
  if (0
      == nr_php_object_instanceof_class(
          response, "Guzzle\\Http\\Message\\Response" TSRMLS_CC)) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Guzzle 3: Request::getResponse() didn't return a Response object");
    goto leave;
  }

  /*
   * Next, we want to get the request duration so we can set the stop time.
   */
  time = nr_guzzle3_response_get_info("total_time", response TSRMLS_CC);
  if ((NULL == time) || (IS_DOUBLE != Z_TYPE_P(time))) {
    nrl_verbosedebug(NRL_INSTRUMENT, "Guzzle 3: total_time is not a double");
    goto leave;
  }
  duration = (nrtime_t)(Z_DVAL_P(time) * NR_TIME_DIVISOR);

  status = nr_php_call(response, "getStatusCode");

  if (nr_php_is_zval_valid_integer(status)) {
    external_params.status = Z_LVAL_P(status);
  }

  /*
   * We also need the URL to create a useful metric.
   */
  url = nr_php_call(request, "getUrl");
  if (!nr_php_is_zval_valid_string(url)) {
    goto leave;
  }
  external_params.uri = nr_strndup(Z_STRVAL_P(url), Z_STRLEN_P(url));

  /*
   * Grab the X-NewRelic-App-Data response header, if there is one. We don't
   * check for a valid string below as it's not an error if the header doesn't
   * exist (and hence NULL is returned).
   */
  external_params.encoded_response_header
      = nr_guzzle_response_get_header(X_NEWRELIC_APP_DATA, response TSRMLS_CC);

  if (NRPRG(txn) && NRTXN(special_flags.debug_cat)) {
    nrl_verbosedebug(
        NRL_CAT, "CAT: outbound response: transport='Guzzle 3' %s=" NRP_FMT,
        X_NEWRELIC_APP_DATA, NRP_CAT(external_params.encoded_response_header));
  }

  nr_segment_set_timing(segment, segment->start_time, duration);
  nr_segment_external_end(&segment, &external_params);

leave:
  nr_free(external_params.encoded_response_header);
  nr_free(external_params.uri);
  nr_php_zval_free(&response);
  nr_php_zval_free(&time);
  nr_php_zval_free(&url);
  nr_php_zval_free(&status);
}

NR_PHP_WRAPPER(nr_guzzle3_request_setstate) {
  zval* state = NULL;
  zval* this_var = NULL;

  (void)wraprec;

  this_var = nr_php_scope_get(NR_EXECUTE_ORIG_ARGS TSRMLS_CC);
  if (0 == nr_php_is_zval_valid_object(this_var)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  if (nr_guzzle3_in_redirect(TSRMLS_C)) {
    NR_PHP_WRAPPER_CALL;
    goto end;
  }

  /*
   * There are two state transitions we're interested in:
   *
   * 1. STATE_TRANSFER: This indicates that the request is about to be sent. We
   *                    want to get the current time so we can create an
   *                    external metric later and inject our CAT headers.
   * 2. STATE_COMPLETE: This indicates that the request is complete and that
   *                    the response has been received in full. At this point,
   *                    we're going to create the external metric.
   */
  state = nr_php_arg_get(1, NR_EXECUTE_ORIG_ARGS TSRMLS_CC);

  NR_PHP_WRAPPER_CALL;

  if (nr_guzzle3_is_state("STATE_TRANSFER", state, this_var TSRMLS_CC)) {
    nr_guzzle3_request_state_transfer(this_var TSRMLS_CC);
  } else if (nr_guzzle3_is_state("STATE_COMPLETE", state, this_var TSRMLS_CC)) {
    nr_guzzle3_request_state_complete(this_var TSRMLS_CC);
  }

end:
  nr_php_arg_release(&state);
  nr_php_scope_release(&this_var);
}
NR_PHP_WRAPPER_END

void nr_guzzle3_enable(TSRMLS_D) {
  if (0 == NRINI(guzzle_enabled)) {
    return;
  }
  /*
   * Instrument Request::setState() so we can detect when the request is
   * completed and then generate the appropriate external metric.
   */
  nr_php_wrap_user_function(NR_PSTR("Guzzle\\Http\\Message\\Request::setState"),
                            nr_guzzle3_request_setstate TSRMLS_CC);
}
