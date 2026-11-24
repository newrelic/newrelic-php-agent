/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for FrankenPHP worker mode instrumentation (php_frankenphp.c).
 *
 * The tests register a fake frankenphp_handle_request. The fake copies the
 * real function (FrankenPHP v1.13.0, frankenphp.c) up to the point where
 * zend_call_function returns: argument parsing, the not-in-worker-mode throw
 * and the callback call. The agent's own code runs before the real function,
 * on the callback's frame, and after the real function returns, when it ends
 * the request's transaction. Most tests check when that transaction ends.
 *
 * For the tests of what the transaction records from the rest of the
 * request, one mode also copies the parts of FrankenPHP's request setup and
 * teardown that the agent reads: the request method, reporting an uncaught
 * exception and flushing the output. Nothing else of FrankenPHP's request
 * handling is copied.
 */
#include "tlib_php.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#include "nr_attributes.h"
#include "nr_commands.h"
#include "nr_errors.h"
#include "nr_txn.h"
#include "php_agent.h"
#include "php_frankenphp_gate.h"
#include "php_globals.h"
#include "php_user_instrument.h"
#include "util_buffer.h"
#include "util_memory.h"
#include "util_object.h"
#include "util_string_pool.h"
#include "util_strings.h"

#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"

#define BOOTSTRAP_TXN_NAME "OtherTransaction/Custom/frankenphp/worker/bootstrap"

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
  char* error_message;
  char* user_attributes;  /* JSON */
  char* agent_attributes; /* JSON */
  /* The names of the segments other than the root, each followed by "\n". */
  char* segment_names;
} captured_txn_t;

static captured_txn_t captured[CAPTURE_MAX];
static size_t captured_count = 0;

typedef struct {
  const nrtxn_t* txn;
  nrbuf_t* names;
} segment_names_collector_t;

static nr_segment_iter_return_t collect_segment_name(nr_segment_t* segment,
                                                     void* userdata) {
  segment_names_collector_t* collector = (segment_names_collector_t*)userdata;
  const char* name;

  if (segment == collector->txn->segment_root) {
    return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
  }

  name = NRSAFESTR(nr_string_get(collector->txn->trace_strings, segment->name));
  nr_buffer_add(collector->names, name, nr_strlen(name));
  nr_buffer_add(collector->names, NR_PSTR("\n"));
  return NR_SEGMENT_NO_POST_ITERATION_CALLBACK;
}

static char* capture_segment_names(const nrtxn_t* txn) {
  segment_names_collector_t collector
      = {.txn = txn, .names = nr_buffer_create(256, 256)};
  char* names;

  nr_segment_iterate(txn->segment_root, collect_segment_name, &collector);
  names = nr_strndup((const char*)nr_buffer_cptr(collector.names),
                     nr_buffer_len(collector.names));
  nr_buffer_destroy(&collector.names);
  return names;
}

static nr_status_t capture_txndata_tx(const nrtxn_t* txn) {
  if (captured_count < CAPTURE_MAX) {
    captured_txn_t* c = &captured[captured_count];
    nrobj_t* attrs = nr_attributes_user_to_obj(txn->attributes,
                                               NR_ATTRIBUTE_DESTINATION_ALL);
    nrobj_t* agent_attrs = nr_attributes_agent_to_obj(
        txn->attributes, NR_ATTRIBUTE_DESTINATION_ALL);

    c->name = nr_strdup(txn->name);
    c->background = txn->status.background;
    c->has_error = (NULL != txn->error);
    c->error_message = nr_strdup(nr_error_get_message(txn->error));
    c->user_attributes = nro_to_json(attrs);
    c->agent_attributes = nro_to_json(agent_attrs);
    c->segment_names = capture_segment_names(txn);
    nro_delete(attrs);
    nro_delete(agent_attrs);
  }
  captured_count += 1;
  return NR_SUCCESS;
}

static void capture_reset(void) {
  size_t i;

  for (i = 0; i < CAPTURE_MAX; i++) {
    nr_free(captured[i].name);
    nr_free(captured[i].error_message);
    nr_free(captured[i].user_attributes);
    nr_free(captured[i].agent_attributes);
    nr_free(captured[i].segment_names);
  }
  nr_memset(captured, 0, sizeof(captured));
  captured_count = 0;
}

/* Whether the captured segment names include the given name. */
static bool segment_names_have(const char* segment_names, const char* name) {
  char* haystack = nr_formatf("\n%s", NRBLANKSTR(segment_names));
  char* needle = nr_formatf("\n%s\n", name);
  bool found = (NULL != nr_strstr(haystack, needle));

  nr_free(haystack);
  nr_free(needle);
  return found;
}
/* }}} */

/* {{{ Fake frankenphp_handle_request */

typedef enum {
  FAKE_RETURN_ONLY,      /* parse the argument, return fake.return_value */
  FAKE_CALL,             /* parse the argument, call the callback once */
  FAKE_CALL_AND_FINISH,  /* as FAKE_CALL, as a web request that the fake
                            then finishes as FrankenPHP does */
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

/*
 * Makes the request a web request. FrankenPHP registers the request's server
 * variables before calling the callback; without REQUEST_METHOD the agent
 * makes the transaction a background job, which records no response
 * attributes.
 */
static void fake_start_web_request(void) {
  php_register_variable("REQUEST_METHOD", "GET",
                        &PG(http_globals)[TRACK_VARS_SERVER]);
}

/*
 * Finishes the request as FrankenPHP v1.13.0 does once zend_call_function
 * returns: it reports a pending exception, then flushes every output buffer
 * (frankenphp.c#L901-L921, #L513-L519). Reporting an uncaught exception sets
 * the status to 500 only with display_errors off and headers not yet sent,
 * which tlib's engine doesn't have, so the fake sets it.
 */
static void fake_finish_request(void) {
  zend_object* exception = EG(exception);

  if (NULL != exception && !zend_is_unwind_exit(exception)
      && !zend_is_graceful_exit(exception)) {
    SG(sapi_headers).http_response_code = 500;
    zend_exception_error(exception, E_ERROR);
  }
  php_output_end_all();
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
 * and, in FAKE_CALL_AND_FINISH mode, what it does after that.
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

  if (FAKE_CALL == fake.mode || FAKE_CALL_AND_FINISH == fake.mode) {
    zval retval;
    zval arg;

    if (FAKE_CALL_AND_FINISH == fake.mode) {
      fake_start_web_request();
    }

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

    if (call < FAKE_MAX_CALLS) {
      fake_observe(&fake.after_call[call]);
      fake.exception_after_call[call] = (NULL != EG(exception));
    }

    /*
     * FAKE_CALL leaves EG(exception) alone, for the test to see the agent's
     * state when zend_call_function returns.
     */
    if (FAKE_CALL_AND_FINISH == fake.mode) {
      fake_finish_request();
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

#define CASE_COUNT(cases) (sizeof(cases) / sizeof((cases)[0]))

/*
 * Creates an engine with the fake registered and transactions captured. The
 * test SAPI registers the fake at module startup, where FrankenPHP registers
 * the real one (through its own module entry); the agent then wraps it at the
 * first RINIT (nr_php_late_initialization) like any other internal function.
 * Registering it after startup instead leaves it with engine state no real
 * function has, and calling it crashes the engine.
 */
static void engine_create_version(const char* extra_ini, const char* version) {
  char* ini = nr_formatf("%s%s", ENGINE_TEST_INI, NRBLANKSTR(extra_ini));

  tlib_php_engine_create_ex(ini, fake_frankenphp_functions);
  nr_free(ini);

  /*
   * The engine's SAPI is embed, so MINIT took the process for something else.
   * Say what the agent would have read from FrankenPHP at MINIT, as a server
   * reload does when it runs MINIT again.
   */
  nr_php_frankenphp_set_process_info("frankenphp", version);
  nr_cmd_txndata_hook = capture_txndata_tx;
  capture_reset();
  fake_reset(FAKE_RETURN_ONLY);
}

static void engine_create(const char* extra_ini) {
  engine_create_version(extra_ini, "v1.13.1");
}

static void engine_destroy(void) {
  tlib_php_engine_destroy();
  capture_reset();
}

/*
 * The kinds of script run the agent has to tell apart at RINIT. Each sets the
 * two signals it reads: the request URI, which has no leading slash for a
 * worker bootstrap and has one for an HTTP request, and the FRANKENPHP_WORKER
 * environment variable, which is 1 on a worker thread.
 */
typedef enum {
  RUN_WORKER_BOOTSTRAP,       /* worker.php, FRANKENPHP_WORKER=1 */
  RUN_CLASSIC,                /* /index.php, variable unset */
  RUN_CLASSIC_WORKER_ENV_SET, /* /index.php, FRANKENPHP_WORKER=1 */
  RUN_BOOTSTRAP_URI_NO_ENV,   /* worker.php, variable unset */
  RUN_NO_SIGNALS              /* no request URI, variable unset */
} run_kind_t;

/*
 * Starts a request with the given request URI and FRANKENPHP_WORKER value,
 * either of which may be NULL, with an empty capture and the fake in the given
 * mode.
 *
 * Both signals are set before tlib_php_request_start(). RINIT forces $_SERVER,
 * and the embed SAPI imports the environment into it then (php_embed.c,
 * php_embed_register_variables). Neither SAPI.c nor main.c touches
 * SG(request_info).request_uri, so the test owns it.
 */
static void request_start_with_signals(char* uri,
                                       const char* worker_env,
                                       fake_mode_t mode) {
  capture_reset();
  fake_reset(mode);
  SG(request_info).request_uri = uri;
  if (NULL != worker_env) {
    setenv("FRANKENPHP_WORKER", worker_env, 1);
  } else {
    unsetenv("FRANKENPHP_WORKER");
  }
  tlib_php_request_start();
}

/*
 * Starts a request that looks like the given kind of run, with an empty
 * capture and the fake in the given mode.
 */
static void request_start_as(run_kind_t kind, fake_mode_t mode) {
  char* uri = NULL;
  const char* worker_env = NULL;

  switch (kind) {
    case RUN_WORKER_BOOTSTRAP:
      uri = "worker.php";
      worker_env = "1";
      break;
    case RUN_CLASSIC:
      uri = "/index.php";
      break;
    case RUN_CLASSIC_WORKER_ENV_SET:
      uri = "/index.php";
      worker_env = "1";
      break;
    case RUN_BOOTSTRAP_URI_NO_ENV:
      uri = "worker.php";
      break;
    case RUN_NO_SIGNALS:
      break;
  }

  request_start_with_signals(uri, worker_env, mode);
}

/*
 * As request_start_as, for a request that has a method. tlib's transactions
 * are background jobs unless REQUEST_METHOD is set, because the agent takes a
 * request that has argv and no REQUEST_METHOD for a script run from the
 * command line. A test that asserts that a transaction is a background job,
 * or that it keeps its type, has to start the request with this, or the
 * assertion passes without the code under test. FrankenPHP's dummy bootstrap
 * request is a GET too.
 */
static void request_start_web_as(run_kind_t kind, fake_mode_t mode) {
  setenv("REQUEST_METHOD", "GET", 1);
  request_start_as(kind, mode);
}

/* Starts a request that looks like a worker bootstrap. */
static void request_start(fake_mode_t mode) {
  request_start_as(RUN_WORKER_BOOTSTRAP, mode);
}

/* Ends the request, and takes away what request_start_as set up. */
static void request_end(void) {
  tlib_php_request_end();
  SG(request_info).request_uri = NULL;
  unsetenv("FRANKENPHP_WORKER");
  unsetenv("REQUEST_METHOD");
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

  request_end();
  engine_destroy();
}

/*
 * Harness check: a request started as a worker bootstrap has both signals the
 * gate reads, the environment variable in $_SERVER, and request_end takes
 * them away again.
 */
static void test_harness_worker_bootstrap_signals(void) {
  engine_create("");
  request_start_as(RUN_WORKER_BOOTSTRAP, FAKE_RETURN_ONLY);

  tlib_pass_if_int_equal("the process reads as FrankenPHP", 1,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_str_equal("the process has a FrankenPHP version", "v1.13.1",
                         NR_PHP_PROCESS_GLOBALS(frankenphp_version));
  pass_if_php_string("the variable reaches $_SERVER", "1",
                     "$_SERVER['FRANKENPHP_WORKER']");
  tlib_pass_if_str_equal("the request URI is the worker script", "worker.php",
                         SG(request_info).request_uri);

  request_end();
  tlib_pass_if_null("request_end clears the request URI",
                    SG(request_info).request_uri);
  tlib_pass_if_null("request_end unsets the variable",
                    getenv("FRANKENPHP_WORKER"));
  engine_destroy();
}

/*
 * Harness check: each kind of run has its own signals, and the web variant
 * adds a request method that the plain one doesn't have, and that doesn't
 * outlive the request.
 */
static void test_harness_run_kinds(void) {
  static const struct {
    run_kind_t kind;
    const char* name;
    const char* uri;
    const char* worker_env;
  } kinds[] = {
      {RUN_WORKER_BOOTSTRAP, "worker bootstrap", "worker.php", "1"},
      {RUN_CLASSIC, "classic", "/index.php", "unset"},
      {RUN_CLASSIC_WORKER_ENV_SET, "classic with the variable", "/index.php",
       "1"},
      {RUN_BOOTSTRAP_URI_NO_ENV, "bootstrap URI without the variable",
       "worker.php", "unset"},
      {RUN_NO_SIGNALS, "no signals", NULL, "unset"},
  };
  const char* method_expr
      = "isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] "
        ": 'unset'";
  size_t i;

  engine_create("");

  for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
    char msg[128];

    request_start_as(kinds[i].kind, FAKE_RETURN_ONLY);

    snprintf(msg, sizeof(msg), "%s: the request URI", kinds[i].name);
    tlib_pass_if_str_equal(msg, kinds[i].uri, SG(request_info).request_uri);
    snprintf(msg, sizeof(msg), "%s: the variable", kinds[i].name);
    pass_if_php_string(msg, kinds[i].worker_env,
                       "isset($_SERVER['FRANKENPHP_WORKER']) "
                       "? $_SERVER['FRANKENPHP_WORKER'] : 'unset'");
    snprintf(msg, sizeof(msg), "%s: no request method", kinds[i].name);
    pass_if_php_string(msg, "unset", method_expr);

    request_end();
  }

  request_start_web_as(RUN_CLASSIC, FAKE_RETURN_ONLY);
  pass_if_php_string("web variant: a GET", "GET", method_expr);
  request_end();

  request_start_as(RUN_CLASSIC, FAKE_RETURN_ONLY);
  pass_if_php_string("after a web request: no request method", "unset",
                     method_expr);
  request_end();

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

  request_end();
  engine_destroy();
}

/*
 * RINIT names the worker bootstrap transaction and makes it a background job
 * as soon as it begins it, so the name holds however the bootstrap ends. The
 * request is a GET, as FrankenPHP's dummy bootstrap request is, so the
 * transaction would otherwise be a web transaction.
 */
static void test_bootstrap_named_at_rinit(void) {
  engine_create("");
  request_start_web_as(RUN_WORKER_BOOTSTRAP, FAKE_RETURN_ONLY);

  tlib_pass_if_not_null("named at RINIT: a transaction", NRPRG(txn));
  if (NULL != NRPRG(txn)) {
    tlib_pass_if_str_equal("named at RINIT: path",
                           "frankenphp/worker/bootstrap", NRPRG(txn)->path);
    tlib_pass_if_int_equal("named at RINIT: path type", NR_PATH_TYPE_CUSTOM,
                           (int)NRPRG(txn)->status.path_type);
    tlib_pass_if_int_equal("named at RINIT: background", 1,
                           NRPRG(txn)->status.background);
  }

  request_end();
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

  request_end();
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

  request_end();
  engine_destroy();

  /*
   * The ignored bootstrap transaction is ended before the real function
   * runs, so the request's transaction doesn't replace it, which would leak
   * it. Valgrind checks the leak.
   */
  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "newrelic_ignore_transaction();"
      "$cb = function () {};"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_true("ignored txn: no transaction when the real function is "
                    "entered",
                    fake.on_entry[0].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal("ignored txn: the request's transaction captured",
                            1, captured_count);
  tlib_fail_if_str_equal("ignored txn: captured one is not the bootstrap",
                         BOOTSTRAP_TXN_NAME, captured[0].name);

  request_end();
  engine_destroy();
}

/*
 * A bootstrap transaction that the worker script ended early with
 * newrelic_end_of_transaction() is sent before the real function runs, as
 * classic mode sends an ended transaction at the end of the request. Ending
 * it froze its name, so it keeps the name it had then: the bootstrap name
 * that RINIT gave it, or the one the worker script gave it.
 */
static void test_bootstrap_ended_early_is_sent(void) {
  static const struct {
    const char* label;
    const char* code;
    const char* expected_name;
  } cases[] = {
      {"ended bootstrap, unnamed",
       "newrelic_end_of_transaction();"
       "$cb = function () { return 1; };"
       "frankenphp_handle_request($cb);",
       BOOTSTRAP_TXN_NAME},
      {"ended bootstrap, named",
       "newrelic_name_transaction('ended_early');"
       "newrelic_end_of_transaction();"
       "$cb = function () { return 1; };"
       "frankenphp_handle_request($cb);",
       "OtherTransaction/Custom/ended_early"},
  };
  size_t i;

  for (i = 0; i < CASE_COUNT(cases); i++) {
    char msg[128];

    engine_create("");
    request_start(FAKE_CALL);

    tlib_php_request_eval(cases[i].code);

    snprintf(msg, sizeof(msg), "%s: bootstrap plus the request",
             cases[i].label);
    tlib_pass_if_size_t_equal(msg, 2, captured_count);
    snprintf(msg, sizeof(msg), "%s: sent before the real function",
             cases[i].label);
    tlib_pass_if_size_t_equal(msg, 1, fake.on_entry[0].captured);
    snprintf(msg, sizeof(msg),
             "%s: no transaction when the real function is entered",
             cases[i].label);
    tlib_pass_if_true(msg, fake.on_entry[0].txn_is_null,
                      "NRPRG(txn) was not NULL");
    snprintf(msg, sizeof(msg), "%s: keeps the name it ended with",
             cases[i].label);
    tlib_pass_if_str_equal(msg, cases[i].expected_name, captured[0].name);

    request_end();
    engine_destroy();
  }
}

/*
 * A worker script that fails before its first frankenphp_handle_request()
 * call still sends its bootstrap transaction, at the end of the script run,
 * under the name RINIT gave it and with the error.
 */
static void test_failed_boot_recorded_as_bootstrap(void) {
  engine_create("");
  request_start_web_as(RUN_WORKER_BOOTSTRAP, FAKE_CALL);

  tlib_php_request_eval("newrelic_notice_error('boot failed');");
  request_end();

  tlib_pass_if_size_t_equal("failed boot: one transaction", 1, captured_count);
  tlib_pass_if_str_equal("failed boot: the bootstrap", BOOTSTRAP_TXN_NAME,
                         captured[0].name);
  tlib_pass_if_true("failed boot: background", captured[0].background,
                    "background=%d", captured[0].background);
  tlib_pass_if_true("failed boot: has the error", captured[0].has_error,
                    "the transaction has no error");
  tlib_pass_if_str_equal("failed boot: the error message", "boot failed",
                         captured[0].error_message);

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

  request_end();
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

  request_end();
  engine_destroy();
}

/*
 * Agent disabled for this request, with the wrapper already installed by
 * an earlier, enabled request. The always-delegate skips the recording check,
 * so the agent's code still runs. A request that is disabled from the start
 * never reaches late initialization, so nothing would be wrapped.
 */
static void test_agent_disabled_for_request(void) {
  zend_function* func = NULL;
  zval* rv = NULL;

  engine_create("");
  request_start(FAKE_RETURN_ONLY);
  request_end();

  NRINI(enabled) = 0;
  request_start(FAKE_CALL);

  func = nr_php_find_function("frankenphp_handle_request");
  tlib_pass_if_true("disabled agent: the agent's wrapper is installed",
                    NULL != func
                        && zif_frankenphp_handle_request
                               != func->internal_function.handler,
                    "frankenphp_handle_request was not wrapped");
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
  tlib_pass_if_int_equal("disabled agent: the thread's role is still unknown",
                         NR_PHP_FRANKENPHP_THREAD_UNKNOWN,
                         nr_php_frankenphp_thread_role());

  request_end();
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

/*
 * phpversion stands in for any internal function because it takes no
 * required arguments: the fake calls the callback with none, and strlen, for
 * one, would throw.
 */
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
    request_end();
    nr_free(code);
  }
  engine_destroy();
}

/*
 * Callable types the agent doesn't wrap. Pinned: current behaviour, not
 * necessarily the desired one. They get no per-request transactions, and
 * nothing says so. Checked by behaviour, because nr_php_get_wraprec must not
 * be given an internal function. Each is a valid argument, though, so the
 * first call still ends the bootstrap before the real function runs.
 */
static void test_rejected_callable_types(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(rejected_callables); i++) {
    const char* label = rejected_callables[i].label;
    char* code = nr_formatf(
        "$cb = %s; frankenphp_handle_request($cb); "
        "frankenphp_handle_request($cb);",
        rejected_callables[i].expr);
    char msg[128];

    request_start(FAKE_CALL);
    tlib_php_request_eval(callable_setup);
    tlib_php_request_eval(code);
    tlib_pass_if_int_equal(rejected_callables[i].label, 2, fake.entered);
    tlib_pass_if_false(rejected_callables[i].label,
                       fake.exception_after_call[0], "the callback threw");
    tlib_pass_if_size_t_equal(rejected_callables[i].label, 1, captured_count);
    snprintf(msg, sizeof(msg),
             "%s: the bootstrap sent before the real function", label);
    tlib_pass_if_size_t_equal(msg, 1, fake.on_entry[0].captured);
    snprintf(msg, sizeof(msg),
             "%s: no transaction when the real function is entered", label);
    tlib_pass_if_true(msg, fake.on_entry[0].txn_is_null,
                      "NRPRG(txn) was not NULL");
    request_end();
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
 * An invalid argument is rejected by the real function, which raises a
 * TypeError or an ArgumentCountError. The agent leaves the bootstrap
 * transaction alone then: it is still open, with its name, when the real
 * function is entered and when the call has thrown, and it is sent at the end
 * of the script run.
 *
 * An invalid call doesn't end the bootstrap, so a valid call after it still
 * does, as the first call on a worker thread always does, and the run records
 * the bootstrap and one worker request. FAKE_RETURN_ONLY never calls the
 * callback, so only this second part shows that the valid call still begins its
 * request's transaction.
 */
static void test_invalid_argument(void) {
  size_t i;

  engine_create("");
  for (i = 0; i < CASE_COUNT(invalid_calls); i++) {
    const char* label = invalid_calls[i].label;
    char* code = nr_formatf(
        "try { %s; $caught = 'nothing'; }"
        " catch (TypeError $e) { $caught = get_class($e); }",
        invalid_calls[i].call);
    char msg[160];

    request_start(FAKE_RETURN_ONLY);
    tlib_php_request_eval(code);
    snprintf(msg, sizeof(msg), "%s: the error class reaches PHP", label);
    pass_if_php_string(msg, invalid_calls[i].expected_error, "$caught");
    snprintf(msg, sizeof(msg), "%s: the real function is entered once", label);
    tlib_pass_if_int_equal(msg, 1, fake.entered);
    snprintf(msg, sizeof(msg), "%s: nothing sent before the real function",
             label);
    tlib_pass_if_size_t_equal(msg, 0, fake.on_entry[0].captured);
    snprintf(msg, sizeof(msg),
             "%s: the bootstrap is open when the real function is entered",
             label);
    tlib_pass_if_false(msg, fake.on_entry[0].txn_is_null,
                       "NRPRG(txn) was NULL");
    snprintf(msg, sizeof(msg), "%s: the bootstrap is still open after the call",
             label);
    tlib_pass_if_not_null(msg, NRPRG(txn));
    if (NULL != NRPRG(txn)) {
      snprintf(msg, sizeof(msg), "%s: the bootstrap keeps its name", label);
      tlib_pass_if_str_equal(msg, "frankenphp/worker/bootstrap",
                             NRPRG(txn)->path);
    }

    request_end();

    snprintf(msg, sizeof(msg), "%s: one transaction at the end of the run",
             label);
    tlib_pass_if_size_t_equal(msg, 1, captured_count);
    snprintf(msg, sizeof(msg), "%s: it is the bootstrap", label);
    tlib_pass_if_str_equal(msg, BOOTSTRAP_TXN_NAME, captured[0].name);
    nr_free(code);
  }

  for (i = 0; i < CASE_COUNT(invalid_calls); i++) {
    const char* label = invalid_calls[i].label;
    char* code = nr_formatf(
        "try { %s; } catch (TypeError $e) {}"
        "$cb = function () { return 1; };"
        "frankenphp_handle_request($cb);",
        invalid_calls[i].call);
    char msg[160];

    request_start(FAKE_CALL);
    tlib_php_request_eval(code);

    snprintf(msg, sizeof(msg),
             "%s, then a valid call: both reach the real function", label);
    tlib_pass_if_int_equal(msg, 2, fake.entered);
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the invalid one sent nothing", label);
    tlib_pass_if_size_t_equal(msg, 0, fake.on_entry[0].captured);
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the bootstrap is open at the invalid one",
             label);
    tlib_pass_if_false(msg, fake.on_entry[0].txn_is_null,
                       "NRPRG(txn) was NULL");
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the valid one sent the bootstrap first",
             label);
    tlib_pass_if_size_t_equal(msg, 1, fake.on_entry[1].captured);
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: no transaction when the valid one is "
             "entered",
             label);
    tlib_pass_if_true(msg, fake.on_entry[1].txn_is_null,
                      "NRPRG(txn) was not NULL");

    request_end();

    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the bootstrap and one worker request",
             label);
    tlib_pass_if_size_t_equal(msg, 2, captured_count);
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the first is the bootstrap", label);
    tlib_pass_if_str_equal(msg, BOOTSTRAP_TXN_NAME, captured[0].name);
    snprintf(msg, sizeof(msg),
             "%s, then a valid call: the second is the worker request", label);
    tlib_fail_if_str_equal(msg, BOOTSTRAP_TXN_NAME, captured[1].name);
    nr_free(code);
  }
  engine_destroy();
}

/*
 * In a classic-mode request that has been started, calls
 * frankenphp_handle_request, which throws as the real function does outside
 * worker mode, then ends the request. The agent leaves the request's
 * transaction alone: it is still open after the call, and it is sent at the
 * end of the request under its own name.
 */
static void pass_if_classic_run_keeps_its_transaction(const char* label) {
  char msg[160];

  tlib_php_request_eval(
      "try { frankenphp_handle_request(function () {}); $caught = 'nothing'; }"
      " catch (RuntimeException $e) { $caught = $e->getMessage(); }");

  snprintf(msg, sizeof(msg), "%s: the real function's exception reaches PHP",
           label);
  pass_if_php_string(
      msg, "frankenphp_handle_request() called while not in worker mode",
      "$caught");
  snprintf(msg, sizeof(msg), "%s: nothing sent during the call", label);
  tlib_pass_if_size_t_equal(msg, 0, captured_count);
  snprintf(msg, sizeof(msg), "%s: the transaction is still open", label);
  tlib_pass_if_not_null(msg, NRPRG(txn));

  request_end();

  snprintf(msg, sizeof(msg), "%s: one transaction at the end of the request",
           label);
  tlib_pass_if_size_t_equal(msg, 1, captured_count);
  snprintf(msg, sizeof(msg), "%s: not named as the bootstrap", label);
  tlib_fail_if_str_equal(msg, BOOTSTRAP_TXN_NAME, captured[0].name);
}

/*
 * Called outside worker mode, the real function throws before it calls the
 * request handler. The rest of the request is recorded as usual.
 */
static void test_called_outside_worker_mode(void) {
  engine_create("");
  request_start_as(RUN_CLASSIC, FAKE_THROW_NOT_WORKER);
  pass_if_classic_run_keeps_its_transaction("outside worker mode");
  engine_destroy();
}

/*
 * The runs in which the two signals disagree, or are both missing, are
 * classic mode too: FRANKENPHP_WORKER set for an HTTP request, a worker
 * script's URI without the variable or with a value other than 1, and a run
 * with no request URI at all.
 */
static void test_classic_runs_keep_their_transaction(void) {
  static const struct {
    const char* label;
    run_kind_t kind;
  } kinds[] = {
      {"classic with the variable", RUN_CLASSIC_WORKER_ENV_SET},
      {"bootstrap URI without the variable", RUN_BOOTSTRAP_URI_NO_ENV},
      {"no signals", RUN_NO_SIGNALS},
  };
  size_t i;

  engine_create("");

  for (i = 0; i < CASE_COUNT(kinds); i++) {
    request_start_as(kinds[i].kind, FAKE_THROW_NOT_WORKER);
    pass_if_classic_run_keeps_its_transaction(kinds[i].label);
  }

  request_start_with_signals("worker.php", "0", FAKE_THROW_NOT_WORKER);
  pass_if_classic_run_keeps_its_transaction(
      "bootstrap URI with FRANKENPHP_WORKER=0");

  engine_destroy();
}

/*
 * The SAPI's environment, as FrankenPHP gives it to sapi_getenv(). Only
 * FRANKENPHP_WORKER is set, to 1.
 */
static char* fake_sapi_getenv(const char* name, size_t name_len) {
  static char one[] = "1";

  if (sizeof("FRANKENPHP_WORKER") - 1 == name_len
      && 0 == nr_strncmp(name, "FRANKENPHP_WORKER", (int)name_len)) {
    return one;
  }
  return NULL;
}

/*
 * When $_SERVER doesn't have FRANKENPHP_WORKER, because variables_order has no
 * S, RINIT reads it from the SAPI's environment for a worker script's URI.
 */
static void test_sapi_getenv_fallback(void) {
  char* (*saved_getenv)(const char* name, size_t name_len) = NULL;

  engine_create("variables_order=EGPC\n");
  saved_getenv = sapi_module.getenv;

  sapi_module.getenv = fake_sapi_getenv;
  request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_RETURN_ONLY);
  pass_if_php_string("getenv fallback: $_SERVER doesn't have the variable",
                     "unset",
                     "isset($_SERVER['FRANKENPHP_WORKER']) "
                     "? $_SERVER['FRANKENPHP_WORKER'] : 'unset'");
  tlib_pass_if_not_null("getenv fallback: a transaction", NRPRG(txn));
  if (NULL != NRPRG(txn)) {
    tlib_pass_if_str_equal("getenv fallback: named as the bootstrap at RINIT",
                           "frankenphp/worker/bootstrap", NRPRG(txn)->path);
  }
  request_end();
  sapi_module.getenv = saved_getenv;

  request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_RETURN_ONLY);
  tlib_pass_if_not_null("without the variable: a transaction", NRPRG(txn));
  if (NULL != NRPRG(txn)) {
    tlib_fail_if_str_equal("without the variable: classic mode",
                           "frankenphp/worker/bootstrap", NRPRG(txn)->path);
  }
  tlib_pass_if_int_equal("without the variable: a classic-mode thread",
                         NR_PHP_FRANKENPHP_THREAD_CLASSIC,
                         nr_php_frankenphp_thread_role());
  request_end();

  engine_destroy();
}

/* How many times the SAPI's environment was read. */
static int sapi_getenv_calls = 0;

/* An empty SAPI environment that counts its reads. */
static char* counting_sapi_getenv(const char* name NRUNUSED,
                                  size_t name_len NRUNUSED) {
  sapi_getenv_calls += 1;
  return NULL;
}

/*
 * On FrankenPHP sapi_getenv() is a call into Go, too costly for every
 * classic-mode RINIT, so RINIT reads the SAPI's environment only for a worker
 * script's URI. With variables_order=EGPC $_SERVER never has
 * FRANKENPHP_WORKER, so any other run would read it too if the URI didn't
 * decide first.
 */
static void test_sapi_getenv_only_for_bootstrap_uri(void) {
  static const struct {
    const char* label;
    run_kind_t kind;
    int expected;
  } kinds[] = {
      {"classic", RUN_CLASSIC, 0},
      {"classic with the variable", RUN_CLASSIC_WORKER_ENV_SET, 0},
      {"no signals", RUN_NO_SIGNALS, 0},
      /* The positive control: the one run that reads it. */
      {"bootstrap URI without the variable", RUN_BOOTSTRAP_URI_NO_ENV, 1},
  };
  char* (*saved_getenv)(const char* name, size_t name_len) = NULL;
  size_t i;

  engine_create("variables_order=EGPC\n");
  saved_getenv = sapi_module.getenv;
  sapi_module.getenv = counting_sapi_getenv;

  for (i = 0; i < CASE_COUNT(kinds); i++) {
    char msg[128];

    sapi_getenv_calls = 0;
    request_start_as(kinds[i].kind, FAKE_RETURN_ONLY);
    snprintf(msg, sizeof(msg), "sapi_getenv, %s: reads at RINIT",
             kinds[i].label);
    tlib_pass_if_int_equal(msg, kinds[i].expected, sapi_getenv_calls);
    request_end();
  }

  sapi_getenv_calls = 0;
  request_start_with_signals("*", NULL, FAKE_RETURN_ONLY);
  tlib_pass_if_int_equal("sapi_getenv, OPTIONS *: reads at RINIT", 0,
                         sapi_getenv_calls);
  request_end();

  sapi_module.getenv = saved_getenv;
  engine_destroy();
}

/*
 * One transaction per call. It is still open when zend_call_function
 * returns, and the agent's wrapper ends it once the real function returns,
 * after FrankenPHP has finished the request. The callback returns a non-null
 * value, which FrankenPHP v1.13.0 forwards (frankenphp.c#L892-L899).
 */
static void test_one_transaction_per_call(void) {
  int i;
  size_t sent_after_return;

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
    /* What the wrapper sent after the real function returned. */
    sent_after_return
        = (i < 2) ? fake.on_entry[i + 1].captured : captured_count;

    tlib_fail_if_str_equal("per-call: request is not the bootstrap",
                           BOOTSTRAP_TXN_NAME, captured[i + 1].name);
    tlib_pass_if_true(
        "per-call: no transaction when the real function is entered",
        fake.on_entry[i].txn_is_null, "NRPRG(txn) was not NULL");
    tlib_pass_if_false("per-call: transaction still open after the call",
                       fake.after_call[i].txn_is_null, "NRPRG(txn) was NULL");
    tlib_pass_if_size_t_equal("per-call: nothing sent during the call",
                              fake.on_entry[i].captured,
                              fake.after_call[i].captured);
    tlib_pass_if_size_t_equal("per-call: one transaction sent after the return",
                              fake.after_call[i].captured + 1,
                              sent_after_return);
    tlib_pass_if_false("per-call: no exception", fake.exception_after_call[i],
                       "the callback threw");
  }

  request_end();
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

  tlib_pass_if_size_t_equal("between requests: no transaction added", 4,
                            captured_count);
  tlib_pass_if_null("between requests: no transaction after the loop",
                    NRPRG(txn));

  request_end();
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
    request_end();
    nr_free(code);
  }
  engine_destroy();
}

/*
 * A closure re-created on each loop pass is recorded on every pass, also when
 * the loop is inside a method. The agent's wrapper wraps each closure before
 * calling the real function, so the request handler's handlers are set by the
 * time Zend installs Observer handlers on the closure's first call. Closures
 * from one declaration in one scope share a runtime cache, so Zend installs
 * them on the first pass only and the later passes reuse them.
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
  request_end();
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
  request_end();
  engine_destroy();

  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "class StaticLoopRunner {"
      "  public function run() {"
      "    for ($i = 0; $i < 3; $i++) {"
      "      frankenphp_handle_request(static function () { return 1; });"
      "    }"
      "  }"
      "}"
      "(new StaticLoopRunner())->run();");
  tlib_pass_if_size_t_equal(
      "closure per pass: static closure in a loop inside a method", 4,
      captured_count);
  request_end();
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

  request_end();
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

  request_end();
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

  request_end();
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
  request_end();
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
  request_end();
  engine_destroy();
}

/*
 * The request handler's begin handler can't begin a transaction, here because
 * no license key is configured, and the request handler begins one with
 * newrelic_start_transaction() and tlib's default license key. That
 * transaction is the request's: it's ended when frankenphp_handle_request
 * returns, not at the next call, where it would be ended as the bootstrap one.
 */
static void test_api_started_txn_after_failed_begin(void) {
  engine_create("newrelic.license=\n");
  request_start(FAKE_CALL_AND_FINISH);

  tlib_pass_if_null("failed begin: no bootstrap transaction", NRPRG(txn));
  tlib_php_request_eval(
      "$cb = function () {"
      "  newrelic_start_transaction(ini_get('newrelic.appname'),"
      "    'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa');"
      "  newrelic_name_transaction('started_by_api');"
      "};"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_true(
      "failed begin: no transaction when the real function is entered",
      fake.on_entry[0].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal(
      "failed begin: sent when frankenphp_handle_request returns", 1,
      captured_count);
  tlib_pass_if_null("failed begin: no transaction left", NRPRG(txn));
  tlib_pass_if_str_equal("failed begin: keeps its name",
                         "WebTransaction/Custom/started_by_api",
                         captured[0].name);
  tlib_pass_if_false("failed begin: not a background job",
                     captured[0].background, "background=%d",
                     captured[0].background);

  request_end();
  engine_destroy();
}

/*
 * The callback throws. The request's transaction is still open when
 * zend_call_function returns, and the agent's wrapper ends it only after the
 * real function returns, which FrankenPHP does after reporting the exception
 * (frankenphp.c#L908). The exception is on it: Zend calls the Observer end
 * handler while unwinding, and the handler records the exception before
 * zend_exception_error would clear it. The fake leaves the exception pending
 * instead of reporting it, so the worker script catches it here.
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
  tlib_pass_if_false("direct throw: transaction still open after the call",
                     fake.after_call[0].txn_is_null, "NRPRG(txn) was NULL");
  tlib_pass_if_size_t_equal(
      "direct throw: only the bootstrap sent during the call", 1,
      fake.after_call[0].captured);
  tlib_pass_if_size_t_equal(
      "direct throw: request transaction sent after the return", 2,
      captured_count);
  tlib_pass_if_true("direct throw: exception recorded on the transaction",
                    captured[1].has_error, "the transaction has no error");
  request_end();
  engine_destroy();

  /*
   * Thrown two frames below the callback, on the first call only, with the
   * same handler reused as real worker loops do. The checks at the
   * boundaries show the wrapper ended the throwing request's transaction
   * before the next call; the total alone would not, because a transaction
   * still open at the next call is ended and sent as the bootstrap one.
   */
  engine_create("");
  request_start(FAKE_CALL);
  depth = NRPRG_CTX(php_cur_stack_depth);
  tlib_php_request_eval(
      "function nested_inner() { throw new RuntimeException('nested'); }"
      "function nested_middle() { nested_inner(); }"
      "$GLOBALS['throw_calls'] = 0;"
      "$cb = function () {"
      "  if (0 === $GLOBALS['throw_calls']++) { nested_middle(); }"
      "};"
      "try { frankenphp_handle_request($cb); } catch (RuntimeException $e) {}"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_true(
      "nested throw: exception pending when zend_call_function returned",
      fake.exception_after_call[0], "no exception");
  tlib_pass_if_false(
      "nested throw: transaction still open after the throwing call",
      fake.after_call[0].txn_is_null, "NRPRG(txn) was NULL");
  tlib_pass_if_size_t_equal(
      "nested throw: only the bootstrap sent during the call", 1,
      fake.after_call[0].captured);
  tlib_pass_if_true("nested throw: no transaction when the next call starts",
                    fake.on_entry[1].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal(
      "nested throw: throwing request's transaction sent first", 2,
      fake.on_entry[1].captured);
  tlib_fail_if_str_equal(
      "nested throw: throwing request's transaction is its own",
      BOOTSTRAP_TXN_NAME, captured[1].name);
  tlib_pass_if_size_t_equal("nested throw: bootstrap plus two requests", 3,
                            captured_count);
  tlib_pass_if_int_equal("nested throw: stack depth balanced", depth,
                         NRPRG_CTX(php_cur_stack_depth));
  request_end();
  engine_destroy();
}

/*
 * frankenphp_handle_request three user frames deep (nested_main, run, once),
 * as in framework runtimes. The outer frames begin in the bootstrap
 * transaction and end after it.
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

  request_end();
  engine_destroy();
}

/*
 * An exception that leaves the request handler is reported by FrankenPHP
 * after the handler returns, which sets the status to 500. The request's
 * transaction must record both: it ends only after the real function
 * returns.
 */
static void test_uncaught_exception_recorded_with_status_500(void) {
  engine_create("");
  request_start(FAKE_CALL_AND_FINISH);

  tlib_php_request_eval(
      "$cb = function () { throw new RuntimeException('handler failed'); };"
      "frankenphp_handle_request($cb);");

  tlib_pass_if_size_t_equal("uncaught exception: bootstrap plus the request",
                            2, captured_count);
  tlib_pass_if_false("uncaught exception: web transaction",
                     captured[1].background, "background=%d",
                     captured[1].background);
  tlib_pass_if_true("uncaught exception: error recorded", captured[1].has_error,
                    "no error on the request's transaction");
  tlib_pass_if_not_null(
      "uncaught exception: the handler's exception",
      nr_strstr(NRSAFESTR(captured[1].error_message), "handler failed"));
  tlib_pass_if_not_null("uncaught exception: status 500",
                        nr_strstr(NRSAFESTR(captured[1].agent_attributes),
                                  "\"http.statusCode\":500"));
  tlib_pass_if_null("uncaught exception: no transaction left", NRPRG(txn));

  request_end();
  engine_destroy();
}

/*
 * An error group callback that throws while the agent records the request
 * handler's uncaught exception. The agent discards the callback's exception
 * and frees it (valgrind checks the leak), the transaction records the
 * handler's exception, and the exception that FrankenPHP reports is still the
 * handler's.
 */
static void test_throwing_error_group_callback(void) {
  zval* callback_ran = NULL;
  zval* reported = NULL;

  engine_create("");
  request_start(FAKE_CALL_AND_FINISH);

  tlib_php_request_eval(
      "$callback_ran = false;"
      "newrelic_set_error_group_callback(function ($txn, $error) {"
      "  $GLOBALS['callback_ran'] = true;"
      "  throw new LogicException('callback failed');"
      "});"
      "$cb = function () { throw new RuntimeException('handler failed'); };"
      "frankenphp_handle_request($cb);");

  callback_ran = tlib_php_request_eval_expr("$callback_ran");
  tlib_pass_if_zval_is_bool_true("throwing callback: the callback ran",
                                 callback_ran);
  tlib_pass_if_size_t_equal("throwing callback: bootstrap plus the request", 2,
                            captured_count);
  tlib_pass_if_true("throwing callback: error recorded", captured[1].has_error,
                    "no error on the request's transaction");
  tlib_pass_if_not_null(
      "throwing callback: the handler's exception is recorded",
      nr_strstr(NRSAFESTR(captured[1].error_message), "handler failed"));

  reported = tlib_php_request_eval_expr("error_get_last()['message'] ?? ''");
  tlib_pass_if_not_null(
      "throwing callback: FrankenPHP reports the handler's exception",
      nr_strstr(Z_STRVAL_P(reported), "handler failed"));
  tlib_pass_if_null(
      "throwing callback: FrankenPHP doesn't report the callback's",
      nr_strstr(Z_STRVAL_P(reported), "callback failed"));
  tlib_pass_if_null("throwing callback: no exception left", EG(exception));

  nr_php_zval_free(&callback_ran);
  nr_php_zval_free(&reported);
  request_end();
  engine_destroy();
}

/*
 * FrankenPHP flushes the output that is still buffered when the request
 * handler returns, and the agent's auto-RUM output handler injects the
 * browser agent into it then. The worker script's own output buffer, started
 * before the call, sits below the agent's handler and receives the response
 * body.
 */
static void test_buffered_html_gets_browser_agent(void) {
  zval* body = NULL;

  engine_create("");
  request_start(FAKE_CALL_AND_FINISH);

  tlib_php_request_eval(
      "$body = '';"
      "ob_start(function ($out) { $GLOBALS['body'] .= $out; return ''; });"
      "$cb = function () {"
      "  echo '<html><head><title>t</title></head><body>ok</body></html>';"
      "};"
      "frankenphp_handle_request($cb);");

  body = tlib_php_request_eval_expr("$body");
  tlib_pass_if_true("browser agent: body captured",
                    NULL != body && IS_STRING == Z_TYPE_P(body),
                    "$body is not a string");
  if (NULL != body && IS_STRING == Z_TYPE_P(body)) {
    tlib_pass_if_not_null("browser agent: request's body",
                          nr_strstr(Z_STRVAL_P(body), "<body>ok"));
    tlib_pass_if_not_null("browser agent: header injected",
                          nr_strstr(Z_STRVAL_P(body), "window.NREUM"));
    tlib_pass_if_not_null("browser agent: footer injected",
                          nr_strstr(Z_STRVAL_P(body), "NREUM.info"));
  }
  nr_php_zval_free(&body);
  tlib_pass_if_size_t_equal("browser agent: bootstrap plus the request", 2,
                            captured_count);

  request_end();
  engine_destroy();
}

/*
 * The request handler is a worker request only when frankenphp_handle_request
 * calls it. Called directly by the worker script, after the agent wrapped it,
 * it must not begin a transaction.
 */
static void test_direct_call_begins_no_transaction(void) {
  engine_create("");
  request_start(FAKE_CALL);

  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "$cb();");

  tlib_pass_if_size_t_equal("direct call: bootstrap plus the request only", 2,
                            captured_count);
  tlib_pass_if_null("direct call: no transaction", NRPRG(txn));

  request_end();
  engine_destroy();
}

/*
 * A transaction started in the worker loop between requests is ended at the
 * next frankenphp_handle_request as it is: not renamed as the bootstrap, and
 * not made a background job, so a named one in a GET keeps its name and its
 * web type. Pinned: interim behaviour, not the desired one. Captured: [0]
 * bootstrap, [1] request 1, [2] the loop-body transaction, [3] request 2.
 */
static void test_loop_body_txn_ended_as_left(void) {
  engine_create("");
  request_start_web_as(RUN_WORKER_BOOTSTRAP, FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "newrelic_start_transaction(ini_get('newrelic.appname'));"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("loop body unnamed: 4 transactions", 4,
                            captured_count);
  tlib_fail_if_str_equal("loop body unnamed: not named as the bootstrap",
                         BOOTSTRAP_TXN_NAME, captured[2].name);
  request_end();
  engine_destroy();

  engine_create("");
  request_start_web_as(RUN_WORKER_BOOTSTRAP, FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "newrelic_start_transaction(ini_get('newrelic.appname'));"
      "newrelic_name_transaction('loop_body');"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("loop body named: 4 transactions", 4,
                            captured_count);
  tlib_pass_if_str_equal("loop body named: keeps its name and type",
                         "WebTransaction/Custom/loop_body", captured[2].name);
  request_end();
  engine_destroy();
}

/*
 * A thread that RINIT took for classic mode is a worker thread when a call to
 * frankenphp_handle_request returns with no exception pending: classic mode
 * throws before calling the request handler. Here RINIT misses the variable,
 * so the begin handler never begins a transaction, and only the return
 * confirms the thread. By then FrankenPHP has set up the first request, so the
 * open transaction has recorded it, and it is discarded.
 */
static void test_confirmed_by_return(void) {
  size_t trampolines = 0;
  size_t i;

  /*
   * Handlers that the begin handler never sees: a trampoline, which the agent
   * can't wrap, and a closure that ran before it was passed in.
   */
  for (i = 0; i < CASE_COUNT(rejected_callables); i++) {
    char* code;
    char msg[128];

    if (NULL == nr_strstr(rejected_callables[i].label, "trampoline")) {
      continue;
    }
    trampolines += 1;
    code = nr_formatf("$cb = %s; frankenphp_handle_request($cb);",
                      rejected_callables[i].expr);

    engine_create("");
    request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_CALL);
    tlib_php_request_eval(callable_setup);
    tlib_php_request_eval(code);

    snprintf(msg, sizeof(msg), "confirmed, %s: no transaction left",
             rejected_callables[i].label);
    tlib_pass_if_null(msg, NRPRG(txn));
    snprintf(msg, sizeof(msg), "confirmed, %s: a worker thread",
             rejected_callables[i].label);
    tlib_pass_if_int_equal(msg, NR_PHP_FRANKENPHP_THREAD_WORKER,
                           nr_php_frankenphp_thread_role());
    snprintf(msg, sizeof(msg), "confirmed, %s: nothing sent",
             rejected_callables[i].label);
    tlib_pass_if_size_t_equal(msg, 0, captured_count);

    request_end();
    engine_destroy();
    nr_free(code);
  }
  tlib_pass_if_size_t_equal("confirmed: both trampoline cases ran", 2,
                            trampolines);

  engine_create("");
  request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "$cb();"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_null("confirmed, warmed-up closure: no transaction left",
                    NRPRG(txn));
  tlib_pass_if_int_equal("confirmed, warmed-up closure: a worker thread",
                         NR_PHP_FRANKENPHP_THREAD_WORKER,
                         nr_php_frankenphp_thread_role());
  tlib_pass_if_size_t_equal("confirmed, warmed-up closure: nothing sent", 0,
                            captured_count);
  request_end();
  engine_destroy();

  /*
   * The request handler replaces the open transaction during the first call.
   * The replacement has recorded the first request too, so it is discarded.
   */
  engine_create("");
  request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_CALL);
  tlib_php_request_eval(
      "class AppnameT {"
      "  public function __call($name, $args) {"
      "    $GLOBALS['replaced']"
      "        = newrelic_set_appname(ini_get('newrelic.appname'));"
      "    return 1;"
      "  }"
      "}"
      "$cb = [new AppnameT(), 'anything'];"
      "frankenphp_handle_request($cb);");
  {
    zval* replaced = tlib_php_request_eval_expr("$replaced");

    tlib_pass_if_zval_is_bool_true(
        "confirmed, set_appname: a replacement began", replaced);
    nr_php_zval_free(&replaced);
  }
  tlib_pass_if_null("confirmed, set_appname: no transaction left", NRPRG(txn));
  tlib_pass_if_int_equal("confirmed, set_appname: a worker thread",
                         NR_PHP_FRANKENPHP_THREAD_WORKER,
                         nr_php_frankenphp_thread_role());
  tlib_pass_if_size_t_equal("confirmed, set_appname: nothing sent", 0,
                            captured_count);
  request_end();
  engine_destroy();

  /*
   * A handler that the begin handler sees. On a thread taken for classic mode
   * it begins no transaction, so the first call is confirmed by its return as
   * above, and the second call is the first worker request with a
   * transaction of its own. Pinned: interim behaviour, not the desired one.
   */
  engine_create("");
  request_start_as(RUN_BOOTSTRAP_URI_NO_ENV, FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("confirmed, plain closure: nothing sent by call 1",
                            0, fake.on_entry[1].captured);
  tlib_pass_if_true(
      "confirmed, plain closure: no transaction when call 2 is entered",
      fake.on_entry[1].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_size_t_equal("confirmed, plain closure: request 2 only", 1,
                            captured_count);
  tlib_fail_if_str_equal("confirmed, plain closure: not the bootstrap",
                         BOOTSTRAP_TXN_NAME, captured[0].name);
  request_end();
  tlib_pass_if_size_t_equal("confirmed, plain closure: nothing more sent", 1,
                            captured_count);
  engine_destroy();
}

/*
 * The functions that test_handler_called_from_user_code_keeps_its_segment
 * passes to frankenphp_handle_request and calls itself.
 */
static const char direct_call_setup[]
    = "function direct_handler() { return 1; }"
      "function other_fn() { return 1; }"
      "function direct_thrower() { throw new LogicException('direct'); }";

/*
 * A request handler that user code calls itself is recorded like any other
 * function the agent wraps: it gets its own segment, in classic mode and in
 * worker mode, and the stack depth stays balanced, also when it throws. A call
 * from frankenphp_handle_request still gets no segment of its own.
 */
static void test_handler_called_from_user_code_keeps_its_segment(void) {
  int depth = 0;

  /* Classic mode. */
  engine_create("");
  request_start_as(RUN_CLASSIC, FAKE_THROW_NOT_WORKER);
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 0;
  tlib_php_request_eval(direct_call_setup);
  depth = NRPRG_CTX(php_cur_stack_depth);
  tlib_php_request_eval(
      "try { frankenphp_handle_request('direct_handler'); }"
      " catch (RuntimeException $e) { direct_handler(); }"
      "other_fn();");
  tlib_pass_if_int_equal("user code, classic: stack depth balanced", depth,
                         NRPRG_CTX(php_cur_stack_depth));
  request_end();
  tlib_pass_if_size_t_equal("user code, classic: one transaction", 1,
                            captured_count);
  tlib_pass_if_true(
      "user code, classic: the positive control has its segment",
      segment_names_have(captured[0].segment_names, "Custom/other_fn"),
      "segment_names=%s", NRSAFESTR(captured[0].segment_names));
  tlib_pass_if_true(
      "user code, classic: the handler has its segment",
      segment_names_have(captured[0].segment_names, "Custom/direct_handler"),
      "segment_names=%s", NRSAFESTR(captured[0].segment_names));
  engine_destroy();

  /* Classic mode, with a handler that throws. */
  engine_create("");
  request_start_as(RUN_CLASSIC, FAKE_THROW_NOT_WORKER);
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 0;
  tlib_php_request_eval(direct_call_setup);
  depth = NRPRG_CTX(php_cur_stack_depth);
  tlib_php_request_eval(
      "try { frankenphp_handle_request('direct_thrower'); }"
      " catch (RuntimeException $e) {}"
      "try { direct_thrower(); } catch (LogicException $e) {}");
  tlib_pass_if_int_equal("user code, throwing: stack depth balanced", depth,
                         NRPRG_CTX(php_cur_stack_depth));
  request_end();
  tlib_pass_if_size_t_equal("user code, throwing: one transaction", 1,
                            captured_count);
  tlib_pass_if_true(
      "user code, throwing: the handler has its segment",
      segment_names_have(captured[0].segment_names, "Custom/direct_thrower"),
      "segment_names=%s", NRSAFESTR(captured[0].segment_names));
  tlib_pass_if_false("user code, throwing: no error on the transaction",
                     captured[0].has_error, "the transaction has an error");
  engine_destroy();

  /*
   * Worker mode. Captured: [0] bootstrap, [1] request 1, [2] the loop-body
   * transaction, [3] request 2.
   */
  engine_create("");
  request_start_as(RUN_WORKER_BOOTSTRAP, FAKE_CALL);
  NR_PHP_PROCESS_GLOBALS(expensive_min) = 0;
  tlib_php_request_eval(direct_call_setup);
  tlib_php_request_eval(
      "frankenphp_handle_request('direct_handler');"
      "newrelic_start_transaction(ini_get('newrelic.appname'));"
      "direct_handler();"
      "frankenphp_handle_request('direct_handler');");
  tlib_pass_if_size_t_equal("user code, worker: 4 transactions", 4,
                            captured_count);
  tlib_pass_if_true(
      "user code, worker: the loop body has the handler's segment",
      segment_names_have(captured[2].segment_names, "Custom/direct_handler"),
      "segment_names=%s", NRSAFESTR(captured[2].segment_names));
  tlib_pass_if_false(
      "user code, worker: request 1 has no segment for the handler",
      segment_names_have(captured[1].segment_names, "Custom/direct_handler"),
      "segment_names=%s", NRSAFESTR(captured[1].segment_names));
  tlib_pass_if_false(
      "user code, worker: request 2 has no segment for the handler",
      segment_names_have(captured[3].segment_names, "Custom/direct_handler"),
      "segment_names=%s", NRSAFESTR(captured[3].segment_names));
  request_end();
  engine_destroy();
}

/*
 * A transaction started in the worker loop between requests and no longer
 * recording at the next frankenphp_handle_request is ended there too, before
 * the real function runs: sent with its own name if the worker script ended
 * it early with newrelic_end_of_transaction(), discarded if it ignored it.
 */
static void test_loop_body_txn_not_recording(void) {
  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "newrelic_start_transaction(ini_get('newrelic.appname'));"
      "newrelic_name_transaction('loop_body');"
      "newrelic_end_of_transaction();"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("loop body ended early: 4 transactions", 4,
                            captured_count);
  tlib_pass_if_size_t_equal(
      "loop body ended early: sent before the real function", 3,
      fake.on_entry[1].captured);
  tlib_pass_if_true(
      "loop body ended early: no transaction when the real function is "
      "entered",
      fake.on_entry[1].txn_is_null, "NRPRG(txn) was not NULL");
  tlib_pass_if_str_equal("loop body ended early: keeps its name",
                         "OtherTransaction/Custom/loop_body", captured[2].name);
  request_end();
  engine_destroy();

  engine_create("");
  request_start(FAKE_CALL);
  tlib_php_request_eval(
      "$cb = function () { return 1; };"
      "frankenphp_handle_request($cb);"
      "newrelic_start_transaction(ini_get('newrelic.appname'));"
      "newrelic_ignore_transaction();"
      "frankenphp_handle_request($cb);");
  tlib_pass_if_size_t_equal("loop body ignored: 3 transactions", 3,
                            captured_count);
  tlib_pass_if_size_t_equal("loop body ignored: nothing sent for it", 2,
                            fake.on_entry[1].captured);
  tlib_pass_if_true(
      "loop body ignored: no transaction when the real function is entered",
      fake.on_entry[1].txn_is_null, "NRPRG(txn) was not NULL");
  request_end();
  engine_destroy();
}
#endif /* ZTS && PHP 8.0+ */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_harness_fake_is_wrapped_and_reachable();
  test_harness_worker_bootstrap_signals();
  test_harness_run_kinds();
  test_bootstrap_txn_ended_before_real_function();
  test_bootstrap_named_at_rinit();
  test_bootstrap_not_recording_app_unknown();
  test_bootstrap_not_recording_txn_ignored();
  test_bootstrap_ended_early_is_sent();
  test_failed_boot_recorded_as_bootstrap();
  test_return_value_passes_through();
  test_same_callable_reuses_wraprec();
  test_agent_disabled_for_request();
  test_wrapped_callable_types();
  test_rejected_callable_types();
  test_invalid_argument();
  test_called_outside_worker_mode();
  test_classic_runs_keep_their_transaction();
  test_sapi_getenv_fallback();
  test_sapi_getenv_only_for_bootstrap_uri();
  test_one_transaction_per_call();
  test_user_code_between_requests_not_recorded();
  test_each_callable_type_gets_transactions();
  test_closure_recreated_each_loop_pass();
  test_warm_up_call_prevents_transactions();
  test_callback_with_a_parameter();
  test_requests_are_isolated();
  test_api_calls_inside_callback();
  test_api_started_txn_after_failed_begin();
  test_callback_throws();
  test_nested_call_site();
  test_uncaught_exception_recorded_with_status_500();
  test_throwing_error_group_callback();
  test_buffered_html_gets_browser_agent();
  test_direct_call_begins_no_transaction();
  test_loop_body_txn_ended_as_left();
  test_confirmed_by_return();
  test_handler_called_from_user_code_keeps_its_segment();
  test_loop_body_txn_not_recording();
#endif
}
