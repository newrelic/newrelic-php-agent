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

static void setup_classes() {
  // clang-format off
  /*
   * A minimal stand-in for Illuminate\Foundation\Application: it only needs
   * to support __construct/boot (so nr_laravel_enable's wraprecs fire) and
   * offsetGet/bind (so nr_laravel_application_boot can look up the bound
   * exception handler, the same way Application's ArrayAccess does).
   */
  const char* application_class =
      "namespace Illuminate\\Foundation;"
      "class Application {"
        "const VERSION = '9.0.0';"
        "private $bindings = [];"
        "function __construct() {}"
        "function boot() {}"
        "function offsetGet($key) {"
          "return isset($this->bindings[$key]) ? $this->bindings[$key] : null;"
        "}"
        "function bind($key, $value) { $this->bindings[$key] = $value; }"
      "}";
  /*
   * laravel_app_handler mirrors the classic app/Exceptions/Handler.php
   * shipped by Laravel <= 10 (and still valid, per Laravel's own upgrade
   * guide, on 11/12/13): a subclass that inherits report()/render() from
   * the framework's base handler without overriding them. On PHP
   * 8+/OAPI, a warm php-fpm worker only recorded the exception on the
   * FIRST request handled by that worker when the bound handler is such an
   * inheriting subclass; every later request on the same worker silently
   * dropped it. Handlers that override, or that are the framework class
   * used directly, recorded correctly on every request. This test drives
   * two requests through one "worker" (one engine, two request cycles) to
   * reproduce that per-worker warm-cache behavior.
   */
  const char* handler_classes =
      "class laravel_base_handler {"
        "function shouldReport($e) { return true; }"
        "function report($e) { return; }"
        "function render($request, $e) { return 'rendered'; }"
      "}"
      "class laravel_app_handler extends laravel_base_handler {}";
  // clang-format on
  tlib_php_request_eval(application_class);
  tlib_php_request_eval(handler_classes);
}

/*
 * Simulates one request served by a warm php-fpm worker: (re)declares the
 * mocked framework/app classes exactly as a fresh request would recompile
 * them, runs Laravel's framework enable + boot lifecycle, binds an
 * ExceptionHandler that only inherits report()/render(), and reports an
 * exception through it. Returns whether the exception was recorded.
 */
static bool simulate_request_and_report_exception(TSRMLS_D) {
  zval* app = NULL;
  zval* handler = NULL;
  zval* key = NULL;
  zval* exception = NULL;
  zval* expr = NULL;
  bool recorded;

  tlib_php_request_start();

  setup_classes();

  NRINI(force_framework) = NR_FW_LARAVEL;
  nr_laravel_enable();

  tlib_pass_if_not_null("Txn should not be null at the start of the request.",
                        NRPRG(txn));
  NRPRG(txn)->options.err_enabled = 1;

  handler = tlib_php_request_eval_expr("new laravel_app_handler()");
  tlib_pass_if_not_null("Mocked exception handler shouldn't be NULL", handler);

  key = tlib_php_request_eval_expr(
      "'Illuminate\\\\Contracts\\\\Debug\\\\ExceptionHandler'");

  /*
   * Application::__construct is wrapped by nr_laravel_enable to install the
   * boot() wrapper, so constructing the mocked application exercises it.
   */
  app = tlib_php_request_eval_expr(
      "new \\Illuminate\\Foundation\\Application()");
  tlib_pass_if_not_null("Mocked application object shouldn't be NULL", app);

  expr = nr_php_call(app, "bind", key, handler);
  nr_php_zval_free(&expr);

  /*
   * Application::boot looks up the bound ExceptionHandler and calls
   * nr_laravel_add_callback_method() to wrap its render()/report() methods.
   * Since laravel_app_handler inherits both from laravel_base_handler, this
   * is exactly the scenario the fix addresses.
   */
  expr = nr_php_call(app, "boot");
  tlib_pass_if_not_null("Application::boot should evaluate", expr);
  nr_php_zval_free(&expr);

  tlib_pass_if_null("No error should be recorded before report() is called",
                    NRTXN(error));

  exception = tlib_php_request_eval_expr("new \\Exception('boom')");

  expr = nr_php_call(handler, "report", exception);
  tlib_pass_if_not_null("report() should evaluate", expr);
  nr_php_zval_free(&expr);

  recorded = (NULL != NRTXN(error));
  if (recorded) {
    tlib_pass_if_str_equal("Recorded error should be the reported exception",
                           "Exception", nr_error_get_klass(NRTXN(error)));
  }

  nr_php_zval_free(&exception);
  nr_php_zval_free(&key);
  nr_php_zval_free(&handler);
  nr_php_zval_free(&app);
  tlib_php_request_end();

  return recorded;
}

static void test_exception_handler_hooks_on_inherited_methods_warm_worker(
    TSRMLS_D) {
  bool recorded_request_1;
  bool recorded_request_2;

  tlib_php_engine_create("");

  /*
   * Both requests are served by the same "worker" (one engine instance),
   * With PHP 8+'s Observer API, the render()/report() wraprec is only ever
   * attached once per worker (nruserfn_t::is_wrapped is never reset between
   * requests), so the second request must reuse the *same* wrap installed by
   * the first. That only works if the wrap was attached to the class that
   * actually defines the method -- attaching it to the concrete subclass
   * (pre-fix) leaves later requests silently uninstrumented.
   */
  recorded_request_1 = simulate_request_and_report_exception();
  tlib_pass_if_true(
      "The first request on a warm worker should record the exception",
      recorded_request_1, "recorded=%d", recorded_request_1);

  recorded_request_2 = simulate_request_and_report_exception();
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
#endif /* PHP 8.0+ */
}
