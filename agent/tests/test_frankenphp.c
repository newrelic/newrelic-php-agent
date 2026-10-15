/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for FrankenPHP worker mode instrumentation (php_frankenphp.c).
 *
 * The tests register a fake frankenphp_handle_request. The fake copies the
 * real function (FrankenPHP v1.13.0, frankenphp.c) only up to the point where
 * zend_call_function returns: argument parsing, the not-in-worker-mode throw
 * and the callback call. It deliberately does not copy what FrankenPHP does
 * around the call (request setup, exception reporting, request teardown).
 * The agent's own code runs only before the real function and on the
 * callback's frame, so that is all these tests need.
 */
#include "tlib_php.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#include "nr_attributes.h"
#include "nr_commands.h"
#include "nr_txn.h"
#include "php_agent.h"
#include "php_globals.h"
#include "php_user_instrument.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_strings.h"

#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"

#define BOOTSTRAP_TXN_NAME "OtherTransaction/Custom/frankenphp/worker"

/*
 * PHP 8.5 always loads OPcache, so every engine these tests create maps
 * OPcache's shared memory. After several engines in one process, valgrind
 * loses track of the address space and crashes (seen with valgrind 3.25.1 on
 * amd64 musl). These tests don't need OPcache, which isn't loaded on PHP 8.4
 * and older, so turn it off.
 */
#if ZEND_MODULE_API_NO >= ZEND_8_5_X_API_NO
#define ENGINE_TEST_INI "opcache.enable=0\n"
#else
#define ENGINE_TEST_INI ""
#endif

/* {{{ Transaction capture */

#define CAPTURE_MAX 16

typedef struct {
  char* name;
  int background;
  bool has_error;
  char* user_attributes; /* JSON */
} captured_txn_t;

static captured_txn_t captured[CAPTURE_MAX];
static size_t captured_count = 0;

static nr_status_t capture_txndata_tx(const nrtxn_t* txn) {
  if (captured_count < CAPTURE_MAX) {
    captured_txn_t* c = &captured[captured_count];
    nrobj_t* attrs = nr_attributes_user_to_obj(txn->attributes,
                                               NR_ATTRIBUTE_DESTINATION_ALL);

    c->name = nr_strdup(txn->name);
    c->background = txn->status.background;
    c->has_error = (NULL != txn->error);
    c->user_attributes = nro_to_json(attrs);
    nro_delete(attrs);
  }
  captured_count += 1;
  return NR_SUCCESS;
}

static void capture_reset(void) {
  size_t i;

  for (i = 0; i < CAPTURE_MAX; i++) {
    nr_free(captured[i].name);
    nr_free(captured[i].user_attributes);
  }
  nr_memset(captured, 0, sizeof(captured));
  captured_count = 0;
}
/* }}} */

/* {{{ Fake frankenphp_handle_request */

typedef enum {
  FAKE_RETURN_ONLY,      /* parse the argument, return fake.return_value */
  FAKE_CALL,             /* parse the argument, call the callback once */
  FAKE_THROW_NOT_WORKER, /* parse the argument, throw as outside worker mode */
} fake_mode_t;

#define FAKE_MAX_CALLS 8

typedef struct {
  bool txn_is_null;
  size_t captured;
} fake_observation_t;

static struct {
  fake_mode_t mode;
  bool return_value;
  bool pass_arg;
  int entered;
  fake_observation_t on_entry[FAKE_MAX_CALLS];
  fake_observation_t after_call[FAKE_MAX_CALLS];
  bool exception_after_call[FAKE_MAX_CALLS];
} fake;

static void fake_reset(fake_mode_t mode) {
  nr_memset(&fake, 0, sizeof(fake));
  fake.mode = mode;
  fake.return_value = true;
}

static void fake_observe(fake_observation_t* observation) {
  observation->txn_is_null = (NULL == NRPRG(txn));
  observation->captured = captured_count;
}

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_frankenphp_handle_request,
                                        0,
                                        1,
                                        _IS_BOOL,
                                        0)
ZEND_ARG_TYPE_INFO(0, callback, IS_CALLABLE, 0)
ZEND_END_ARG_INFO()

/*
 * Mirrors frankenphp_handle_request() from FrankenPHP v1.13.0 up to the
 * return of zend_call_function:
 * https://github.com/php/frankenphp/blob/v1.13.0/frankenphp.c#L841-L892
 * The real function parses with Z_PARAM_FUNC; "f" raises the same TypeError
 * and ArgumentCountError.
 */
static ZEND_FUNCTION(frankenphp_handle_request) {
  zend_fcall_info fci;
  zend_fcall_info_cache fcc;
  int call = fake.entered;

  fake.entered += 1;
  if (call < FAKE_MAX_CALLS) {
    fake_observe(&fake.on_entry[call]);
  }

  if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "f", &fci, &fcc)) {
    RETURN_THROWS();
  }

  if (FAKE_THROW_NOT_WORKER == fake.mode) {
    zend_throw_exception(
        spl_ce_RuntimeException,
        "frankenphp_handle_request() called while not in worker mode", 0);
    RETURN_THROWS();
  }

  if (FAKE_CALL == fake.mode) {
    zval retval;
    zval arg;

    ZVAL_UNDEF(&retval);
    ZVAL_STRING(&arg, "fake-arg");
    fci.size = sizeof(fci);
    fci.retval = &retval;
    fci.params = fake.pass_arg ? &arg : NULL;
    fci.param_count = fake.pass_arg ? 1 : 0;

    if (SUCCESS == zend_call_function(&fci, &fcc)
        && IS_UNDEF != Z_TYPE(retval)) {
      zval_ptr_dtor(&retval);
    }
    zval_ptr_dtor(&arg);

    /*
     * EG(exception) is left alone: what FrankenPHP does with it can only be
     * tested against a real FrankenPHP.
     */
    if (call < FAKE_MAX_CALLS) {
      fake_observe(&fake.after_call[call]);
      fake.exception_after_call[call] = (NULL != EG(exception));
    }
    RETURN_TRUE;
  }

  RETURN_BOOL(fake.return_value);
}

static const zend_function_entry fake_frankenphp_functions[]
    = {ZEND_FE(frankenphp_handle_request, arginfo_frankenphp_handle_request)
           ZEND_FE_END};
/* }}} */

/* {{{ Helpers */

/*
 * Creates an engine with the fake registered and transactions captured. The
 * test SAPI registers the fake at module startup, the way a real SAPI
 * registers its own functions; the agent then wraps it at the first RINIT
 * (nr_php_late_initialization) like any other internal function. Registering
 * it after startup instead leaves it with engine state no real function has,
 * and calling it crashes the engine.
 */
static void engine_create(const char* extra_ini) {
  char* ini = nr_formatf("%s%s", ENGINE_TEST_INI, NRBLANKSTR(extra_ini));

  tlib_php_engine_create_ex(ini, fake_frankenphp_functions);
  nr_free(ini);
  nr_cmd_txndata_hook = capture_txndata_tx;
  capture_reset();
  fake_reset(FAKE_RETURN_ONLY);
}

static void engine_destroy(void) {
  tlib_php_engine_destroy();
  capture_reset();
}

/* Starts a request with an empty capture and the fake in the given mode. */
static void request_start(fake_mode_t mode) {
  capture_reset();
  fake_reset(mode);
  tlib_php_request_start();
}

/*
 * The wraprec of the callable the PHP expression evaluates to. Only for
 * callables that are user functions: nr_php_get_wraprec reads op_array
 * fields.
 */
static nruserfn_t* wraprec_of(const char* expr) {
  zval* callable = tlib_php_request_eval_expr(expr);
  nruserfn_t* wr = nr_php_get_wraprec(nr_php_zval_to_function(callable));

  nr_php_zval_free(&callable);
  return wr;
}

static bool wraprec_has_custom_handlers(const nruserfn_t* wr) {
  return NULL != wr && NULL != wr->fcall_handlers.begin
         && NULL != wr->fcall_handlers.end;
}

/* Passes if the PHP expression evaluates to the expected string. */
static void pass_if_php_string(const char* msg,
                               const char* expected,
                               const char* expr) {
  zval* value = tlib_php_request_eval_expr(expr);

  tlib_pass_if_str_equal(
      msg, expected,
      (NULL != value && IS_STRING == Z_TYPE_P(value)) ? Z_STRVAL_P(value)
                                                       : "<not a string>");
  nr_php_zval_free(&value);
}
/* }}} */

/*
 * Harness check: the fake is in the function table, the agent wrapped it at
 * the first RINIT, and PHP code reaches it.
 */
static void test_harness_fake_is_wrapped_and_reachable(void) {
  zend_function* func = NULL;
  zval* rv = NULL;

  engine_create("");
  request_start(FAKE_RETURN_ONLY);

  func = nr_php_find_function("frankenphp_handle_request");
  tlib_pass_if_not_null("fake is in the function table", func);
  tlib_pass_if_true("the agent replaced the fake's handler",
                    NULL != func
                        && zif_frankenphp_handle_request
                               != func->internal_function.handler,
                    "frankenphp_handle_request was not wrapped");

  rv = tlib_php_request_eval_expr("frankenphp_handle_request(function () {})");
  tlib_pass_if_int_equal("fake entered once", 1, fake.entered);
  tlib_pass_if_zval_is_bool_true("fake's return value reaches PHP", rv);
  nr_php_zval_free(&rv);

  tlib_php_request_end();
  engine_destroy();
}

/* The bootstrap transaction ends before the real function runs. */
static void test_bootstrap_txn_ended_before_real_function(void) {
  engine_create("");
  request_start(FAKE_RETURN_ONLY);

  tlib_php_request_eval("frankenphp_handle_request(function () {});");

  tlib_pass_if_size_t_equal("bootstrap: one transaction captured", 1,
                            captured_count);
  tlib_pass_if_str_equal("bootstrap: name", BOOTSTRAP_TXN_NAME,
                         captured[0].name);
  tlib_pass_if_true("bootstrap: is background", captured[0].background,
                    "background=%d", captured[0].background);
  tlib_pass_if_true(
      "bootstrap: no transaction when the real function is entered",
      fake.on_entry[0].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal("bootstrap: sent before the real function", 1,
                            fake.on_entry[0].captured);

  tlib_php_request_end();
  engine_destroy();
}

static nr_status_t mock_cmd_appinfo_unknown(nrapp_t* app) {
  app->state = NR_APP_UNKNOWN;
  return NR_SUCCESS;
}

/* Not recording at bootstrap because the app is unknown. */
static void test_bootstrap_not_recording_app_unknown(void) {
  engine_create("");
  nr_cmd_appinfo_hook = mock_cmd_appinfo_unknown;
  request_start(FAKE_RETURN_ONLY);

  tlib_pass_if_false("app unknown: not recording", nr_php_recording(),
                     "expected the agent not to be recording");
  tlib_php_request_eval(
      "$cb = function () {};"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_size_t_equal("app unknown: nothing captured", 0, captured_count);
  tlib_pass_if_true("app unknown: callable still wrapped",
                    wraprec_has_custom_handlers(wraprec_of("$cb")),
                    "no wraprec with custom handlers");

  tlib_php_request_end();
  engine_destroy();
}

/* Not recording at bootstrap because the transaction is ignored. */
static void test_bootstrap_not_recording_txn_ignored(void) {
  engine_create("");
  request_start(FAKE_RETURN_ONLY);

  tlib_php_request_eval(
      "newrelic_ignore_transaction();"
      "$cb = function () {};"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_size_t_equal("ignored txn: nothing captured", 0, captured_count);
  tlib_pass_if_true("ignored txn: callable still wrapped",
                    wraprec_has_custom_handlers(wraprec_of("$cb")),
                    "no wraprec with custom handlers");

  tlib_php_request_end();
  engine_destroy();
}

/* The real function's return value reaches PHP unchanged. */
static void test_return_value_passes_through(void) {
  zval* rv = NULL;

  engine_create("");
  request_start(FAKE_RETURN_ONLY);

  fake.return_value = false;
  rv = tlib_php_request_eval_expr("frankenphp_handle_request(function () {})");
  tlib_pass_if_zval_is_bool_false("return value: false passes through", rv);
  nr_php_zval_free(&rv);

  fake.return_value = true;
  rv = tlib_php_request_eval_expr("frankenphp_handle_request(function () {})");
  tlib_pass_if_zval_is_bool_true("return value: true passes through", rv);
  nr_php_zval_free(&rv);

  tlib_php_request_end();
  engine_destroy();
}

/* The same callable passed twice reuses its wraprec. */
static void test_same_callable_reuses_wraprec(void) {
  nruserfn_t* first = NULL;
  nruserfn_t* second = NULL;

  engine_create("");
  request_start(FAKE_RETURN_ONLY);

  tlib_php_request_eval(
      "$cb = function () {};"
      "frankenphp_handle_request($cb);");
  first = wraprec_of("$cb");
  tlib_php_request_eval("frankenphp_handle_request($cb);");
  second = wraprec_of("$cb");

  tlib_pass_if_not_null("same callable: wrapped", first);
  tlib_pass_if_ptr_equal("same callable: same wraprec reused", first, second);

  tlib_php_request_end();
  engine_destroy();
}

/* Agent disabled for this request. */
static void test_agent_disabled_for_request(void) {
  zval* rv = NULL;

  engine_create("");
  NRINI(enabled) = 0;
  request_start(FAKE_CALL);

  rv = tlib_php_request_eval_expr(
      "frankenphp_handle_request(function () { "
      "$GLOBALS['callback_ran'] = true; })");
  tlib_pass_if_int_equal("disabled agent: fake entered", 1, fake.entered);
  tlib_pass_if_zval_is_bool_true("disabled agent: return value passes through",
                                 rv);
  nr_php_zval_free(&rv);
  rv = tlib_php_request_eval_expr("$callback_ran");
  tlib_pass_if_zval_is_bool_true("disabled agent: callback ran", rv);
  nr_php_zval_free(&rv);
  tlib_pass_if_size_t_equal("disabled agent: nothing captured", 0,
                            captured_count);

  tlib_php_request_end();
  engine_destroy();
}

/* {{{ Callables */

/*
 * PHP declarations used by the callable-type cases. Evaluated at the start of
 * each request, because user functions and classes don't outlive a request.
 */
static const char callable_setup[]
    = "function cb_fn() { return 1; }"
      "class CbC {"
      "  public static function sm() { return 1; }"
      "  public function m() { return 1; }"
      "  public function __invoke() { return 1; }"
      "  public function closureInMethod() {"
      "    return function () { return $this; };"
      "  }"
      "  public function staticClosureInMethod() {"
      "    return static function () { return 1; };"
      "  }"
      "}"
      "class CbT {"
      "  public function __call($name, $args) { return 1; }"
      "  public static function __callStatic($name, $args) { return 1; }"
      "}"
      "$o = new CbC();"
      "$t = new CbT();";

typedef struct {
  const char* label;
  const char* expr;
} callable_case_t;

#define CASE_COUNT(cases) (sizeof(cases) / sizeof((cases)[0]))

static const callable_case_t wrapped_callables[] = {
    {"closure", "function () { return 1; }"},
    {"arrow function", "fn () => 1"},
    {"function name", "'cb_fn'"},
    {"static method string", "'CbC::sm'"},
    {"static method array", "['CbC', 'sm']"},
    {"instance method array", "[$o, 'm']"},
    {"invokable object", "$o"},
    {"first-class callable function", "cb_fn(...)"},
    {"first-class callable method", "$o->m(...)"},
    {"Closure::fromCallable", "Closure::fromCallable('cb_fn')"},
    /* Closures created inside a method. */
    {"closure created in a method", "$o->closureInMethod()"},
    {"static closure created in a method", "$o->staticClosureInMethod()"},
};

static const callable_case_t rejected_callables[] = {
    {"internal function name", "'phpversion'"},
    {"internal first-class callable", "phpversion(...)"},
    {"__call trampoline", "[$t, 'anything']"},
    {"__callStatic trampoline", "['CbT', 'anything']"},
};
/* }}} */

/* Callable types that get a wraprec with the custom handlers. */
static void test_wrapped_callable_types(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(wrapped_callables); i++) {
    char* code = nr_formatf("$cb = %s; frankenphp_handle_request($cb);",
                            wrapped_callables[i].expr);

    request_start(FAKE_RETURN_ONLY);
    tlib_php_request_eval(callable_setup);
    tlib_php_request_eval(code);
    tlib_pass_if_true(wrapped_callables[i].label,
                      wraprec_has_custom_handlers(wraprec_of("$cb")),
                      "no wraprec with custom handlers");
    tlib_php_request_end();
    nr_free(code);
  }
  engine_destroy();
}

/*
 * Callable types the agent doesn't wrap. Pinned: current behaviour, not
 * necessarily the desired one. They get no per-request transactions, and
 * nothing says so. Checked by behaviour, because nr_php_get_wraprec must not
 * be given an internal function.
 */
static void test_rejected_callable_types(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(rejected_callables); i++) {
    char* code = nr_formatf(
        "$cb = %s; frankenphp_handle_request($cb); "
        "frankenphp_handle_request($cb);",
        rejected_callables[i].expr);

    request_start(FAKE_CALL);
    tlib_php_request_eval(callable_setup);
    tlib_php_request_eval(code);
    tlib_pass_if_int_equal(rejected_callables[i].label, 2, fake.entered);
    tlib_pass_if_false(rejected_callables[i].label,
                       fake.exception_after_call[0], "the callback threw");
    tlib_pass_if_size_t_equal(rejected_callables[i].label, 1, captured_count);
    tlib_php_request_end();
    nr_free(code);
  }
  engine_destroy();
}

typedef struct {
  const char* label;
  const char* call;
  const char* expected_error;
} invalid_call_case_t;

static const invalid_call_case_t invalid_calls[] = {
    {"no argument", "frankenphp_handle_request()", "ArgumentCountError"},
    {"integer", "frankenphp_handle_request(42)", "TypeError"},
    {"unknown function name", "frankenphp_handle_request('no_such_fn')",
     "TypeError"},
};

/*
 * Invalid argument. Pinned: current behaviour, not necessarily the desired
 * one. The agent ends and renames the bootstrap transaction before the
 * argument is known to be valid.
 */
static void test_invalid_argument(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(invalid_calls); i++) {
    char* code = nr_formatf(
        "try { %s; $caught = 'nothing'; }"
        " catch (TypeError $e) { $caught = get_class($e); }",
        invalid_calls[i].call);

    request_start(FAKE_RETURN_ONLY);
    tlib_php_request_eval(code);
    pass_if_php_string(invalid_calls[i].label, invalid_calls[i].expected_error,
                       "$caught");
    tlib_pass_if_int_equal(invalid_calls[i].label, 1, fake.entered);
    tlib_pass_if_size_t_equal(invalid_calls[i].label, 1, captured_count);
    tlib_pass_if_str_equal(invalid_calls[i].label, BOOTSTRAP_TXN_NAME,
                           captured[0].name);
    tlib_php_request_end();
    nr_free(code);
  }
  engine_destroy();
}

/*
 * Called outside worker mode. Pinned: current behaviour, not necessarily the
 * desired one. The current transaction is ended and renamed before the real
 * function throws, so the rest of the request goes unrecorded.
 */
static void test_called_outside_worker_mode(void) {
  engine_create("");
  request_start(FAKE_THROW_NOT_WORKER);

  tlib_php_request_eval(
      "try { frankenphp_handle_request(function () {}); $caught = 'nothing'; }"
      " catch (RuntimeException $e) { $caught = $e->getMessage(); }");

  pass_if_php_string(
      "outside worker mode: the real function's exception reaches PHP",
      "frankenphp_handle_request() called while not in worker mode",
      "$caught");
  tlib_pass_if_size_t_equal(
      "outside worker mode: current transaction already sent", 1,
      captured_count);
  tlib_pass_if_str_equal("outside worker mode: and renamed", BOOTSTRAP_TXN_NAME,
                         captured[0].name);
  tlib_pass_if_null("outside worker mode: rest of the request unrecorded",
                    NRPRG(txn));

  tlib_php_request_end();
  engine_destroy();
}

/*
 * One transaction per call, ended before control returns to the real
 * function. The callback returns a non-null value, which FrankenPHP v1.13.0
 * forwards (frankenphp.c#L892-L899).
 */
static void test_one_transaction_per_call(void) {
  int i;

  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "$cb = function () { return ['status' => 200]; };"
      "for ($i = 0; $i < 3; $i++) { frankenphp_handle_request($cb); }");

  tlib_pass_if_int_equal("per-call: fake entered three times", 3, fake.entered);
  tlib_pass_if_size_t_equal("per-call: bootstrap plus three requests", 4,
                            captured_count);
  tlib_pass_if_str_equal("per-call: first is the bootstrap", BOOTSTRAP_TXN_NAME,
                         captured[0].name);
  for (i = 0; i < 3; i++) {
    tlib_fail_if_str_equal("per-call: request is not the bootstrap",
                           BOOTSTRAP_TXN_NAME, captured[i + 1].name);
    tlib_pass_if_true("per-call: no transaction after the call",
                      fake.after_call[i].txn_is_null,
                      "NRPRG(txn) was not NULL");
    tlib_pass_if_size_t_equal("per-call: the call sent one transaction",
                              fake.on_entry[i].captured + 1,
                              fake.after_call[i].captured);
    tlib_pass_if_false("per-call: no exception", fake.exception_after_call[i],
                       "the callback threw");
  }

  tlib_php_request_end();
  engine_destroy();
}

/* User code in the worker loop between requests is not recorded. */
static void test_user_code_between_requests_not_recorded(void) {
  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "function between_requests() { return 1; }"
      "$cb = function () { return 1; };"
      "for ($i = 0; $i < 3; $i++) {"
      "  frankenphp_handle_request($cb);"
      "  between_requests();"
      "}");

  tlib_pass_if_size_t_equal(
      "between requests: code between requests adds nothing", 4,
      captured_count);
  tlib_pass_if_null("between requests: no transaction after the loop",
                    NRPRG(txn));

  tlib_php_request_end();
  engine_destroy();
}

/* Every wrapped callable type gets one transaction per call. */
static void test_each_callable_type_gets_transactions(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(wrapped_callables); i++) {
    char* code = nr_formatf(
        "$cb = %s; frankenphp_handle_request($cb); "
        "frankenphp_handle_request($cb);",
        wrapped_callables[i].expr);

    request_start(FAKE_CALL);
    tlib_php_request_eval(callable_setup);
    tlib_php_request_eval(code);
    tlib_pass_if_size_t_equal(wrapped_callables[i].label, 3, captured_count);
    tlib_php_request_end();
    nr_free(code);
  }
  engine_destroy();
}

/*
 * A closure re-created on each loop pass shares one runtime cache (same
 * declaration, same scope), so every pass is recorded, also when the loop is
 * inside a method.
 */
static void test_closure_recreated_each_loop_pass(void) {
  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "for ($i = 0; $i < 3; $i++) {"
      "  frankenphp_handle_request(function () { return 1; });"
      "}");
  tlib_pass_if_size_t_equal("closure per pass: top-level loop", 4,
                            captured_count);
  tlib_php_request_end();
  engine_destroy();

  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "class LoopRunner {"
      "  public function run() {"
      "    for ($i = 0; $i < 3; $i++) {"
      "      frankenphp_handle_request(function () { return $this; });"
      "    }"
      "  }"
      "}"
      "(new LoopRunner())->run();");
  tlib_pass_if_size_t_equal("closure per pass: loop inside a method", 4,
                            captured_count);
  tlib_php_request_end();
  engine_destroy();
}

/*
 * An assumption of the instrumentation, pinned. Zend installs Observer
 * handlers on a function's first call, so a callable that ran before it was
 * passed in keeps the default handlers and never gets per-request
 * transactions. Real worker scripts (FrankenPHP's example, Laravel Octane,
 * Symfony Runtime) pass a closure that only frankenphp_handle_request ever
 * calls, so they never hit this.
 */
static void test_warm_up_call_prevents_transactions(void) {
  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "$cb();"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("warmed-up closure: bootstrap only", 1,
                            captured_count);

  tlib_php_request_end();
  engine_destroy();
}

/*
 * FrankenPHP v1.13.0 passes the callback one argument when it has one
 * (frankenphp.c#L889-L890).
 */
static void test_callback_with_a_parameter(void) {
  engine_create("");
  request_start(FAKE_CALL);
  fake.pass_arg = true;

  tlib_php_request_eval(
      "$cb = function ($payload) { $GLOBALS['got_payload'] = $payload; };"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_size_t_equal("callback parameter: one transaction per call", 3,
                            captured_count);
  pass_if_php_string("callback parameter: the callback got the argument",
                     "fake-arg", "$got_payload");

  tlib_php_request_end();
  engine_destroy();
}

/* Nothing set in one request leaks into the next. */
static void test_requests_are_isolated(void) {
  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "$GLOBALS['request_calls'] = 0;"
      "$cb = function () {"
      "  if (0 === $GLOBALS['request_calls']++) {"
      "    newrelic_name_transaction('named_first');"
      "    newrelic_add_custom_parameter('first_key', 'first_value');"
      "    newrelic_notice_error('first request error');"
      "  }"
      "};"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_size_t_equal("isolation: bootstrap plus two requests", 3,
                            captured_count);
  /* Positive controls: request 1 really has all three. */
  tlib_pass_if_not_null("isolation: request 1 named",
                        nr_strstr(captured[1].name, "named_first"));
  tlib_pass_if_not_null("isolation: request 1 attribute",
                        nr_strstr(captured[1].user_attributes, "first_key"));
  tlib_pass_if_true("isolation: request 1 error", captured[1].has_error,
                    "no error on request 1");
  tlib_pass_if_null("isolation: request 2 not named",
                    nr_strstr(captured[2].name, "named_first"));
  tlib_pass_if_null("isolation: request 2 no attribute",
                    nr_strstr(captured[2].user_attributes, "first_key"));
  tlib_pass_if_false("isolation: request 2 no error", captured[2].has_error,
                     "request 2 has an error");

  tlib_php_request_end();
  engine_destroy();
}

/* Agent API calls that end or ignore the request's transaction. */
static void test_api_calls_inside_callback(void) {
  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "$GLOBALS['calls'] = 0;"
      "$cb = function () {"
      "  if (0 === $GLOBALS['calls']++) { newrelic_ignore_transaction(); }"
      "};"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal(
      "ignore_transaction: ignored request not captured, the next is", 2,
      captured_count);
  tlib_php_request_end();
  engine_destroy();

  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { newrelic_end_transaction(); };"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("end_transaction: one capture per request", 3,
                            captured_count);
  tlib_pass_if_null("end_transaction: no transaction left", NRPRG(txn));
  tlib_php_request_end();
  engine_destroy();
}

/*
 * The callback throws. Zend calls the Observer end handler while unwinding,
 * so the request's transaction is already sent when zend_call_function
 * returns. Pinned: current behaviour, not necessarily the desired one. The
 * exception isn't on the transaction. Real FrankenPHP reports the exception
 * only after that (frankenphp.c#L908), so it is probably lost; only a test
 * against a real FrankenPHP can confirm. Real runtimes mostly catch
 * exceptions inside the handler, which makes this the less common path.
 */
static void test_callback_throws(void) {
  int depth = 0;

  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "$cb = function () { throw new RuntimeException('direct'); };"
      "try { frankenphp_handle_request($cb); $caught = 'nothing'; }"
      " catch (RuntimeException $e) { $caught = $e->getMessage(); }");

  pass_if_php_string("direct throw: exception reaches the worker script",
                     "direct", "$caught");
  tlib_pass_if_true(
      "direct throw: exception pending when zend_call_function returned",
      fake.exception_after_call[0], "no exception");
  tlib_pass_if_true("direct throw: no transaction after the call",
                    fake.after_call[0].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal("direct throw: request transaction already sent", 2,
                            fake.after_call[0].captured);
  tlib_pass_if_false("direct throw: exception not recorded on the transaction",
                     captured[1].has_error, "the transaction has an error");
  tlib_php_request_end();
  engine_destroy();

  /* Thrown two frames below the callback. */
  engine_create("");
  request_start(FAKE_CALL);
  depth = NRPRG_CTX(php_cur_stack_depth);
  tlib_php_request_eval(
      "function nested_inner() { throw new RuntimeException('nested'); }"
      "function nested_middle() { nested_inner(); }"
      "$cb = function () { nested_middle(); };"
      "try { frankenphp_handle_request($cb); } catch (RuntimeException $e) {}"
      "frankenphp_handle_request(function () { return 1; });");
  tlib_pass_if_size_t_equal("nested throw: bootstrap plus two requests", 3,
                            captured_count);
  tlib_pass_if_int_equal("nested throw: stack depth balanced", depth,
                         NRPRG_CTX(php_cur_stack_depth));
  tlib_php_request_end();
  engine_destroy();
}

/*
 * frankenphp_handle_request two user frames deep, as in framework
 * runtimes. The outer frames begin in the bootstrap transaction and end
 * after it.
 */
static void test_nested_call_site(void) {
  int depth = 0;

  engine_create("");
  request_start(FAKE_CALL);
  depth = NRPRG_CTX(php_cur_stack_depth);

  tlib_php_request_eval(
      "class NestedRunner {"
      "  public function run(callable $handler) {"
      "    for ($i = 0; $i < 3; $i++) { $this->once($handler); }"
      "  }"
      "  private function once(callable $handler) {"
      "    return frankenphp_handle_request($handler);"
      "  }"
      "}"
      "function nested_main() {"
      "  (new NestedRunner())->run(function () { return 1; });"
      "}"
      "nested_main();");

  tlib_pass_if_size_t_equal("nested call site: bootstrap plus three requests",
                            4, captured_count);
  tlib_pass_if_int_equal("nested call site: stack depth balanced", depth,
                         NRPRG_CTX(php_cur_stack_depth));
  tlib_pass_if_null("nested call site: no transaction after the loop",
                    NRPRG(txn));

  tlib_php_request_end();
  engine_destroy();
}
#endif /* ZTS && PHP 8.0+ */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_harness_fake_is_wrapped_and_reachable();
  test_bootstrap_txn_ended_before_real_function();
  test_bootstrap_not_recording_app_unknown();
  test_bootstrap_not_recording_txn_ignored();
  test_return_value_passes_through();
  test_same_callable_reuses_wraprec();
  test_agent_disabled_for_request();
  test_wrapped_callable_types();
  test_rejected_callable_types();
  test_invalid_argument();
  test_called_outside_worker_mode();
  test_one_transaction_per_call();
  test_user_code_between_requests_not_recorded();
  test_each_callable_type_gets_transactions();
  test_closure_recreated_each_loop_pass();
  test_warm_up_call_prevents_transactions();
  test_callback_with_a_parameter();
  test_requests_are_isolated();
  test_api_calls_inside_callback();
  test_callback_throws();
  test_nested_call_site();
#endif
}
