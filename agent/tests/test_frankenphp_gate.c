/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Unit tests for the FrankenPHP worker-mode gate (php_frankenphp_gate.c).
 *
 * These tests need no engine. They call the gate's functions directly and
 * read the process globals they set, so each test wraps its work in
 * nr_php_global_init() and nr_php_global_destroy().
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
#endif /* ZTS && PHP 8.0+ */

void test_main(void* p NRUNUSED) {
#if defined(ZTS) && ZEND_MODULE_API_NO >= ZEND_8_0_X_API_NO
  test_parse_version();
  test_process_info();
  test_log_once();
#endif
}
