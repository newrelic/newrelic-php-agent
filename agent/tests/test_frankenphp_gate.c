/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for the FrankenPHP worker-mode gate (php_frankenphp_gate.c).
 *
 * Most of these tests need no engine. They call the gate's functions directly
 * and read the process globals they set, so those that touch the process
 * globals wrap their work in nr_php_global_init() and
 * nr_php_global_destroy().
 *
 * The test of the INI setting creates engines, and tlib runs MINIT and
 * MSHUTDOWN itself, which set up and tear down the process globals. It runs
 * after the others.
 */
#include "tlib_php.h"

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
#include "php_agent.h"
#include "php_frankenphp_gate.h"
#include "php_globals.h"
#include "util_memory.h"
#include "util_strings.h"

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

typedef struct {
  const char* input;
  bool ok;
  int major;
  int minor;
  int patch;
} parse_row_t;

static void test_parse_version(void) {
  static const parse_row_t rows[] = {
      {"v1.13.1", true, 1, 13, 1}, {"1.13.0", true, 1, 13, 0},
      {"v1.2.4", true, 1, 2, 4},   {"v2.0.0", true, 2, 0, 0},
      {"dev", false, 0, 0, 0},     {"", false, 0, 0, 0},
      {NULL, false, 0, 0, 0},      {"vv1.2.3", false, 0, 0, 0},
      {"V1.2.3", false, 0, 0, 0},  {"1.2", false, 0, 0, 0},
      {"1.2.3.4", false, 0, 0, 0}, {"v1.13.1-rc.1", false, 0, 0, 0},
      {" 1.2.3", false, 0, 0, 0},  {"1.2.3 ", false, 0, 0, 0},
      {"1..3", false, 0, 0, 0},    {"1.-2.3", false, 0, 0, 0},
      {"1.2.-3", false, 0, 0, 0},  {"+1.2.3", false, 0, 0, 0},
      {"1. 2.3", false, 0, 0, 0},  {"99999999999.0.0", false, 0, 0, 0},
      {"abc123f", false, 0, 0, 0}, {"1.13.1+build", false, 0, 0, 0},
  };
  size_t i;
  nr_php_frankenphp_version_t untouched = {-1, -1, -1};

  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    char what[128];
    nr_php_frankenphp_version_t out = untouched;
    bool ok;

    snprintf(what, sizeof(what), "parse_version(%s)",
             NULL == rows[i].input ? "NULL" : rows[i].input);

    ok = nr_php_frankenphp_parse_version(rows[i].input, &out);
    tlib_pass_if_bool_equal(what, rows[i].ok, ok);

    if (rows[i].ok) {
      tlib_pass_if_int_equal(what, rows[i].major, out.major);
      tlib_pass_if_int_equal(what, rows[i].minor, out.minor);
      tlib_pass_if_int_equal(what, rows[i].patch, out.patch);
    } else {
      tlib_pass_if_int_equal(what, untouched.major, out.major);
      tlib_pass_if_int_equal(what, untouched.minor, out.minor);
      tlib_pass_if_int_equal(what, untouched.patch, out.patch);
    }
  }

  tlib_pass_if_bool_equal("parse_version with no output", false,
                          nr_php_frankenphp_parse_version("v1.2.3", NULL));
}

static void test_process_info(void) {
  nr_php_global_init();

  nr_php_frankenphp_set_process_info("frankenphp", "v1.11.2");
  tlib_pass_if_int_equal("frankenphp SAPI", 1,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_str_equal("frankenphp SAPI keeps the version", "v1.11.2",
                         NR_PHP_PROCESS_GLOBALS(frankenphp_version));

  /*
   * A second call is what a server reload does: the new version replaces the
   * old one, and the copy of the old one is freed (valgrind checks that).
   */
  nr_php_frankenphp_set_process_info("frankenphp", "v1.13.1");
  tlib_pass_if_int_equal("second call: still frankenphp", 1,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_str_equal("second call: the new version wins", "v1.13.1",
                         NR_PHP_PROCESS_GLOBALS(frankenphp_version));

  nr_php_frankenphp_set_process_info("frankenphp", NULL);
  tlib_pass_if_int_equal("no version: still frankenphp", 1,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_null("no version: the old version is gone",
                    NR_PHP_PROCESS_GLOBALS(frankenphp_version));

  nr_php_frankenphp_set_process_info("frankenphp", "v1.13.1");
  nr_php_frankenphp_set_process_info("embed", "v1.13.1");
  tlib_pass_if_int_equal("other SAPI", 0,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_null("other SAPI: no version, whatever was passed",
                    NR_PHP_PROCESS_GLOBALS(frankenphp_version));

  nr_php_frankenphp_set_process_info("frankenphp", "v1.13.1");
  nr_php_frankenphp_set_process_info(NULL, "v1.13.1");
  tlib_pass_if_int_equal("no SAPI name", 0,
                         NR_PHP_PROCESS_GLOBALS(is_frankenphp));
  tlib_pass_if_null("no SAPI name: no version",
                    NR_PHP_PROCESS_GLOBALS(frankenphp_version));

  /* Left set for nr_php_global_destroy() to free. */
  nr_php_frankenphp_set_process_info("frankenphp", "v1.2.4");

  nr_php_global_destroy();
}

static void test_log_once(void) {
  nr_php_global_init();
  nr_php_frankenphp_set_process_info("frankenphp", "v1.13.1");

  tlib_pass_if_bool_equal(
      "first call", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF));
  tlib_pass_if_bool_equal(
      "second call", false,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF));
  tlib_pass_if_bool_equal(
      "third call", false,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF));

  /* Each kind of line has its own flag. */
  tlib_pass_if_bool_equal(
      "another kind: first call", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_WORKER_ENV_SET));
  tlib_pass_if_bool_equal(
      "another kind: second call", false,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_WORKER_ENV_SET));
  tlib_pass_if_bool_equal(
      "a kind not yet logged", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_NO_WORKER_ENV));
  tlib_pass_if_bool_equal(
      "unknown version: first call", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_UNKNOWN_VERSION));
  tlib_pass_if_bool_equal(
      "missed at RINIT: first call", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT));
  tlib_pass_if_bool_equal(
      "missed at RINIT: second call", false,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_MISSED_AT_RINIT));

  /* A server reload runs MINIT again, which clears every flag. */
  nr_php_frankenphp_set_process_info("frankenphp", "v1.13.1");
  tlib_pass_if_bool_equal(
      "after set_process_info: first call", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF));
  tlib_pass_if_bool_equal(
      "after set_process_info: second call", false,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_OFF));
  tlib_pass_if_bool_equal(
      "after set_process_info: another kind", true,
      nr_php_frankenphp_log_once(NR_PHP_FRANKENPHP_LOG_WORKER_ENV_SET));

  nr_php_global_destroy();
}

typedef struct {
  const char* uri;
  const char* env;
  nr_php_frankenphp_run_t expected;
} classify_row_t;

static void test_classify_run(void) {
  static const classify_row_t rows[] = {
      /* A bootstrap-shaped URI and FRANKENPHP_WORKER exactly "1". */
      {"worker.php", "1", NR_PHP_FRANKENPHP_RUN_BOOTSTRAP},
      {"public/worker.php", "1", NR_PHP_FRANKENPHP_RUN_BOOTSTRAP},
      {"x", "1", NR_PHP_FRANKENPHP_RUN_BOOTSTRAP},

      /* An HTTP request, or a run with no signals at all. */
      {"/index.php", NULL, NR_PHP_FRANKENPHP_RUN_CLASSIC},
      {"*", NULL, NR_PHP_FRANKENPHP_RUN_CLASSIC},
      {"", NULL, NR_PHP_FRANKENPHP_RUN_CLASSIC},
      {NULL, NULL, NR_PHP_FRANKENPHP_RUN_CLASSIC},

      /* The variable is "1" but the URI isn't bootstrap-shaped. */
      {"/index.php", "1", NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET},
      {"/", "1", NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET},
      {"*", "1", NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET},
      {"", "1", NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET},
      {NULL, "1", NR_PHP_FRANKENPHP_RUN_CLASSIC_WORKER_ENV_SET},

      /* The URI is bootstrap-shaped but the variable isn't exactly "1". */
      {"worker.php", NULL, NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV},
      {"worker.php", "0", NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV},
      {"worker.php", "true", NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV},
      {"worker.php", "", NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV},
      {"worker.php", " 1", NR_PHP_FRANKENPHP_RUN_CLASSIC_NO_WORKER_ENV},
  };
  size_t i;

  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    char what[128];

    snprintf(what, sizeof(what), "classify_run(uri=%s, env=%s)",
             NULL == rows[i].uri ? "NULL" : rows[i].uri,
             NULL == rows[i].env ? "NULL" : rows[i].env);

    tlib_pass_if_int_equal(
        what, rows[i].expected,
        nr_php_frankenphp_classify_run(rows[i].uri, rows[i].env));
  }
}

typedef struct {
  nr_php_frankenphp_setting_t setting;
  const char* version;
  nr_php_frankenphp_mode_t expected;
} mode_row_t;

static void test_mode_for(void) {
  static const char* const setting_names[] = {"auto", "on", "off"};
  static const mode_row_t rows[] = {
      /* auto: instrumented from v1.11.2, unsupported below it and when the
         version is unknown. */
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v1.11.2",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v1.13.1",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "1.13.0",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v2.0.0",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v1.11.1",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v1.2.4",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "v1.2.3",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "dev",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, "", NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_AUTO, NULL,
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},

      /* on: instrumented from the technical floor, v1.2.4, and when the
         version is unknown, as on a custom build. */
      {NR_PHP_FRANKENPHP_SETTING_ON, "v1.11.2",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "v1.11.1",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "v1.2.4",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "dev",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "abc123f",
       NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "", NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, NULL, NR_PHP_FRANKENPHP_MODE_INSTRUMENTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "v1.2.3",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},
      {NR_PHP_FRANKENPHP_SETTING_ON, "v1.0.0",
       NR_PHP_FRANKENPHP_MODE_UNSUPPORTED},

      /* off: off, whatever the version. */
      {NR_PHP_FRANKENPHP_SETTING_OFF, "v1.13.1", NR_PHP_FRANKENPHP_MODE_OFF},
      {NR_PHP_FRANKENPHP_SETTING_OFF, "v1.11.1", NR_PHP_FRANKENPHP_MODE_OFF},
      {NR_PHP_FRANKENPHP_SETTING_OFF, "v1.2.3", NR_PHP_FRANKENPHP_MODE_OFF},
      {NR_PHP_FRANKENPHP_SETTING_OFF, NULL, NR_PHP_FRANKENPHP_MODE_OFF},
  };
  size_t i;

  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    char what[128];

    snprintf(what, sizeof(what), "mode_for(%s, %s)",
             setting_names[rows[i].setting],
             NULL == rows[i].version ? "NULL" : rows[i].version);

    tlib_pass_if_int_equal(
        what, rows[i].expected,
        nr_php_frankenphp_mode_for(rows[i].setting, rows[i].version));
  }
}

typedef struct {
  const char* value; /* NULL: no line in the INI at all */
  nr_php_frankenphp_setting_t expected;
} ini_row_t;

/*
 * Creates an engine for each value, so it has to run after the tests that
 * don't use an engine (see the comment at the top of the file).
 *
 * Each value is written after the = exactly as it would be in a php.ini, so
 * the unquoted ones go through the INI parser's keyword rewriting first: it
 * hands the handler "1" for on, yes and true, and an empty string for off, no,
 * false and none. An empty value is therefore off, not auto.
 */
static void test_ini_worker_mode(void) {
  static const ini_row_t rows[] = {
      /* Not set anywhere: the default. */
      {NULL, NR_PHP_FRANKENPHP_SETTING_AUTO},
      /* An invalid value is refused, so the default stays. */
      {"maybe", NR_PHP_FRANKENPHP_SETTING_AUTO},
      /* An empty value, which is also what an unquoted off becomes. */
      {"", NR_PHP_FRANKENPHP_SETTING_OFF},
      {"on", NR_PHP_FRANKENPHP_SETTING_ON},
      {"OFF", NR_PHP_FRANKENPHP_SETTING_OFF},
      {"yes", NR_PHP_FRANKENPHP_SETTING_ON},
      {"true", NR_PHP_FRANKENPHP_SETTING_ON},
      {"no", NR_PHP_FRANKENPHP_SETTING_OFF},
      {"false", NR_PHP_FRANKENPHP_SETTING_OFF},
      {"none", NR_PHP_FRANKENPHP_SETTING_OFF},
      /* The parser hands 0 over as "0", which is off too. */
      {"0", NR_PHP_FRANKENPHP_SETTING_OFF},
      /* Quoted, the parser leaves the value alone. */
      {"\"On\"", NR_PHP_FRANKENPHP_SETTING_ON},
      {"\"OFF\"", NR_PHP_FRANKENPHP_SETTING_OFF},
      {"auto", NR_PHP_FRANKENPHP_SETTING_AUTO},
      {"\"AUTO\"", NR_PHP_FRANKENPHP_SETTING_AUTO},
  };
  size_t i;

  for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
    char what[128];
    char* ini;

    snprintf(what, sizeof(what), "%s=%s", NR_PHP_FRANKENPHP_INI_WORKER_MODE,
             NULL == rows[i].value ? "(no line)" : rows[i].value);

    if (NULL == rows[i].value) {
      ini = nr_strdup(ENGINE_TEST_INI);
    } else {
      char* line = nr_formatf("%s=%s\n", NR_PHP_FRANKENPHP_INI_WORKER_MODE,
                              rows[i].value);

      ini = nr_formatf("%s%s", ENGINE_TEST_INI, line);
      nr_free(line);
    }

    tlib_php_engine_create(ini);
    nr_free(ini);
    tlib_php_request_start();

    tlib_pass_if_int_equal(what, rows[i].expected,
                           NRINI(frankenphp_worker_mode));

    tlib_php_request_end();
    tlib_php_engine_destroy();
  }
}
#endif /* ZTS && PHP 8.0+ */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_parse_version();
  test_process_info();
  test_log_once();
  test_classify_run();
  test_mode_for();

  /* Last: it creates engines. */
  test_ini_worker_mode();
#endif
}
