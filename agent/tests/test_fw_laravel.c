/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "tlib_php.h"

#include "nr_errors.h"
#include "php_agent.h"
#include "php_call.h"
#include "php_wrapper.h"
#include "fw_hooks.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO

/*
 * Simulates one request served by a warm php-fpm worker: (re)declares the
 * mocked framework/app classes exactly as a fresh request would recompile
 * them, runs Laravel's framework enable + boot lifecycle, binds an
 * ExceptionHandler that only inherits report()/render(), and reports an
 * exception through it. Returns whether the exception was recorded.
 */
static bool simulate_request_and_report_exception(const char* script) {
  bool recorded;

  tlib_php_request_start();

  // Simulate a request and report an exception through the Laravel exception handler.
  tlib_php_request_eval(script);
  tlib_pass_if_int_equal("framework detected", NRPRG_SHARED(current_framework), NR_FW_LARAVEL);

  recorded = (NULL != NRTXN(error));

  tlib_php_request_end();

  return recorded;
}

static void test_exception_handler_hooks_on_inherited_methods_warm_worker(
    TSRMLS_D) {
  bool recorded_request_1;
  bool recorded_request_2;

  tlib_php_engine_create("newrelic.special=show_loaded_files");

  /*
   * Both requests are served by the same "worker" (one engine instance),
   * With PHP 8+'s Observer API, the render()/report() wraprec is only ever
   * attached once per worker (nruserfn_t::is_wrapped is never reset between
   * requests), so the second request must reuse the *same* wrap installed by
   * the first. That only works if the wrap was attached to the class that
   * actually defines the method -- attaching it to the concrete subclass
   * (pre-fix) leaves later requests silently uninstrumented.
   */
  recorded_request_1 = simulate_request_and_report_exception("require '" PHP_SCRIPTS_DIR "/laravel/laravel_request_handler_custom.php';");
  tlib_pass_if_true(
      "The first request on a warm worker should record the exception",
      recorded_request_1, "recorded=%d", recorded_request_1);

  recorded_request_2 = simulate_request_and_report_exception("require '" PHP_SCRIPTS_DIR "/laravel/laravel_request_handler_custom.php';");
  tlib_pass_if_true(
      "A later request on the SAME warm worker should also record the "
      "exception, not just the first",
      recorded_request_2, "recorded=%d", recorded_request_2);

  tlib_php_engine_destroy();
}

static void test_exception_handler_hooks_on_base_methods_warm_worker(
    TSRMLS_D) {
  bool recorded_request_1;
  bool recorded_request_2;

  tlib_php_engine_create("newrelic.special=show_loaded_files");

  /*
   * Both requests are served by the same "worker" (one engine instance),
   * With PHP 8+'s Observer API, the render()/report() wraprec is only ever
   * attached once per worker (nruserfn_t::is_wrapped is never reset between
   * requests), so the second request must reuse the *same* wrap installed by
   * the first. That only works if the wrap was attached to the class that
   * actually defines the method -- attaching it to the concrete subclass
   * (pre-fix) leaves later requests silently uninstrumented.
   */
  recorded_request_1 = simulate_request_and_report_exception("require '" PHP_SCRIPTS_DIR "/laravel/laravel_request_handler_base.php';");
  tlib_pass_if_true(
      "The first request on a warm worker should record the exception",
      recorded_request_1, "recorded=%d", recorded_request_1);

  recorded_request_2 = simulate_request_and_report_exception("require '" PHP_SCRIPTS_DIR "/laravel/laravel_request_handler_base.php';");
  tlib_pass_if_true(
      "A later request on the SAME warm worker should also record the "
      "exception, not just the first",
      recorded_request_2, "recorded=%d", recorded_request_2);

  tlib_php_engine_destroy();
}

#endif

void test_main(void* p NRUNUSED) {
#if ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_exception_handler_hooks_on_inherited_methods_warm_worker();
  test_exception_handler_hooks_on_base_methods_warm_worker();
#endif /* PHP 8.0+ */
}
