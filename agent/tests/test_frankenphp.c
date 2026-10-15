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
#endif
}
