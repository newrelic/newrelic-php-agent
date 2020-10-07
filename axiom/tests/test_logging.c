/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "nr_banner.h"
#include "util_logging.h"
#include "util_logging_private.h"
#include "util_memory.h"
#include "util_strings.h"
#include "util_syscalls.h"

#include "tlib_main.h"

/*
 * throw away any strings involving the uid or gid.  This filter is pretty
 * coarse.
 */
const char* cleanup_string
    = "sed "
      "-e 's/ os=[^]]*//g' "
      "-e 's/id=[0-9]*/id=-1/g' "
      "-e 's/New Relic [0-9.]*/New Relic X.Y.Z.W/' "
      "-e 's/(\"[-.a-zA-Z0-9]*\" - \"[0-9a-fA-F]*\")/(\"NAME\" - \"GITSHA\")/' "
      "| sed -e 's/^[^a-fA-F]*[0-9]*) //'";

static void test_set_log_level(void);

static void test_logging(void) {
  int i;
  nr_status_t rv;

  /*
   * Need to ensure we don't start out with existing log files from multiple
   * test runs.
   */
  nr_unlink("logtest1.tmp");
  nr_unlink("logtest2.tmp");

  /*
   * Try sending a log message before anything has been initialized. Should
   * fail.
   */
  rv = nrl_send_log_message(NRL_ALWAYS, "test should fail");
  tlib_pass_if_true("log write before initialization failed", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  /*
   * Try to initialize to an impossible to write to file. Should fail.
   */
  rv = nrl_set_log_file("/should/not/exist");
  tlib_pass_if_true("initialization to bad path fails", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  /*
   * Attempts to log should still fail.
   */
  rv = nrl_send_log_message(NRL_ALWAYS, "test should fail");
  tlib_pass_if_true("log write after failed init fails", NR_FAILURE == rv,
                    "rv=%d", (int)rv);

  /*
   * Open to a valid file should succeed.
   */
  rv = nrl_set_log_file("./logtest1.tmp");
  tlib_pass_if_true("log initialization succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_exists("./logtest1.tmp");

  /*
   * Log message should succeed now.
   */
  rv = nrl_send_log_message(NRL_ALWAYS, "expect PASS 1");
  tlib_pass_if_true("NRL_ALWAYS succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  /*
   * Test the logging macros at the default level. Whether or not these tests
   * succeed will be determined when the log file is compared to the reference
   * version at the end of these tests.
   */
  nrl_always("NRL_ALWAYS should be present (1)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (1)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (1)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (1)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (1)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (1)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (1)");

  /*
   * Test mechanisms to thwart log injection.
   */
  {
    const char* short_appname = "01234";
    const char* long_appname
        = "01234567890123456789012345678901234567890123456789";
    nrl_error(NRL_TEST, "A short appname " NRP_FMT, NRP_APPNAME(short_appname));
    nrl_error(NRL_TEST, "A 50 char appname truncated " NRP_FMT,
              NRP_APPNAME(long_appname)); /* 50 chars */
  }

  /*
   * Test long log messages, which is potentially a bit of a stress for
   * the vasprintf. We have seen cases where asan complains about the
   * free of the vasprintf allocated memory from within the logging
   * engine.
   */
  for (i = 1; i <= (1 << 9); i <<= 1) {
    nrl_error(NRL_TEST, "Variable width int %*d", i, i);
  }

  /*
   * Close the log file.
   */
  nrl_close_log_file();
  tlib_pass_if_int_equal("close log file", -1, nrl_get_log_fd());

  /*
   * A second close attempt should not blow up.
   */
  nrl_close_log_file();
  tlib_pass_if_int_equal("still closed", -1, nrl_get_log_fd());

  /*
   * Writing a log message should fail (file is closed).
   */
  rv = nrl_send_log_message(NRL_ALWAYS, "test should fail");
  tlib_pass_if_true("log write after close failed", NR_FAILURE == rv, "rv=%d",
                    (int)rv);

  /*
   * Reopen the same file. Data should be appended. Logging should succeed.
   */
  rv = nrl_set_log_file("./logtest1.tmp");
  tlib_pass_if_true("log reopen succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_exists("./logtest1.tmp");
  rv = nrl_send_log_message(NRL_ALWAYS, "expect PASS 2");
  tlib_pass_if_true("NRL_ALWAYS succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  /*
   * Put in a banner, with various configurations.
   */
  nr_banner("daemon_location", NR_DAEMON_STARTUP_UNKOWN, "Axiom Tests");
  nr_banner("daemon_location", NR_DAEMON_STARTUP_INIT, "Axiom Tests");
  nr_banner("daemon_location", NR_DAEMON_STARTUP_AGENT, "Axiom Tests");
  nr_banner(0, NR_DAEMON_STARTUP_AGENT, "Axiom Tests");

  /*
   * Retest the macros with newly reopened log file.
   */
  nrl_always("NRL_ALWAYS should be present (2)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (2)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (2)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (2)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (2)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (2)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (2)");

  /*
   * Change the log file name without first closing the old file. Should
   * succeed and messages should be written just fine.
   */
  rv = nrl_set_log_file("./logtest2.tmp");
  tlib_pass_if_true("log change succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_exists("./logtest2.tmp");
  rv = nrl_send_log_message(NRL_ALWAYS, "expect PASS 3");
  tlib_pass_if_true("NRL_ALWAYS succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  nrl_always("NRL_ALWAYS should be present (3)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (3)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (3)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (3)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (3)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (3)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (3)");

  test_set_log_level();

  nrl_close_log_file();
}

/*
 * Now do the tests for setting the log level. We do this by manually
 * inspecting the nrl_level_mask_ptr array after doing the various settings.
 * We will also actually call the macro functions at key points and
 * verify that only the correct messages make it through to the log file
 * when we compare the generated log file to the reference version. We
 * check each and every valid log level first.
 */

static void test_set_log_level_simple(void) {
  nr_status_t rv;

  /*
   * Simple settings of valid levels.
   */
  rv = nrl_set_log_level("error");
  tlib_pass_if_true("set level (error)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = 0", 0 == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = 0", 0 == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (4)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (4)");
  nrl_warning(NRL_TEST, "NRL_WARNING should not be present (4)");
  nrl_info(NRL_TEST, "NRL_INFO should not be present (4)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (4)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (4)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (4)");
}

static void test_set_log_level_null(void) {
  nr_status_t rv;

  /*
   * Passing NULL as the level should default to info.
   */
  rv = nrl_set_log_level(0);
  tlib_pass_if_true("set level (0)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (5)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (5)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (5)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (5)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (5)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (5)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (5)");
}

static void test_set_log_level_warning(void) {
  nr_status_t rv;

  rv = nrl_set_log_level("warning");
  tlib_pass_if_true("set level (warning)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = 0", 0 == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (6)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (6)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (6)");
  nrl_info(NRL_TEST, "NRL_INFO should not be present (6)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (6)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (6)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (6)");
}

static void test_set_log_level_info(void) {
  nr_status_t rv;

  rv = nrl_set_log_level("info");
  tlib_pass_if_true("set level (info)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (7)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (7)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (7)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (7)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (7)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (7)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (7)");
}

static void test_set_log_level_verbose(void) {
  nr_status_t rv;

  rv = nrl_set_log_level("verbose");
  tlib_pass_if_true("set level (verbose)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (8)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (8)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (8)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (8)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should be present (8)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (8)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (8)");
}

static void test_set_log_level_debug(void) {
  nr_status_t rv;

  rv = nrl_set_log_level("debug");
  tlib_pass_if_true("set level (debug)", NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (9)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (9)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (9)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (9)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should be present (9)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should be present (9)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (9)");
}

static void test_set_log_level_verbosedebug(void) {
  nr_status_t rv;

  rv = nrl_set_log_level("verbosedebug");
  tlib_pass_if_true("set level (verbosedebug)", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_VERBOSEDEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (9)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (9)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (9)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (9)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should be present (9)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should be present (9)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should be present (9)");
}

static void test_set_log_level_bogus(void) {
  nr_status_t rv;

  /*
   * Now test a simple invalid level. Should have the effect of falling back
   * to the default (info).
   */
  rv = nrl_set_log_level("bogus");
  tlib_pass_if_true("set level (bogus)", NR_FAILURE == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (10)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (10)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (10)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (10)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (10)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (10)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (10)");
}

static void test_set_log_level_overall(void) {
  nr_status_t rv;

  /*
   * Set an overall level, and turn on extra for some subsystems.
   */
  rv = nrl_set_log_level("warning,autorum=verbose,framework=verbosedebug");
  tlib_pass_if_true(
      "set level (warning,autorum=verbose,framework=verbosedebug)",
      NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true(
      "mask[info] = ALL",
      (NRL_AUTORUM | NRL_FRAMEWORK) == nrl_level_mask_ptr[NRL_INFO],
      "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true(
      "mask[verbose] = 0",
      (NRL_AUTORUM | NRL_FRAMEWORK) == nrl_level_mask_ptr[NRL_VERBOSE],
      "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0",
                    NRL_FRAMEWORK == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    NRL_FRAMEWORK == nrl_level_mask_ptr[NRL_VERBOSEDEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (11)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (11)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (11)");
  nrl_info(NRL_TEST, "NRL_INFO should not be present (11)");
  nrl_info(NRL_AUTORUM, "NRL_INFO(AUTORUM) should be present (11)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (11)");
  nrl_verbose(NRL_FRAMEWORK, "NRL_VERBOSE(FRAMEWORK) should be present (11)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (11)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (11)");
  nrl_verbosedebug(NRL_FRAMEWORK,
                   "NRL_VERBOSEDEBUG(FRAMEWORK) should be present (11)");
}

static void test_set_log_level_invalid_subsystem(void) {
  nr_status_t rv;

  /*
   * Set an invalid sub-system. Should reset everything back to defaults.
   */
  rv = nrl_set_log_level("verbosedebug,bogus=debug");
  tlib_pass_if_true("set level (verbosedebug,bogus=debug)", NR_FAILURE == rv,
                    "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true("mask[info] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_INFO],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true("mask[verbose] = 0", 0 == nrl_level_mask_ptr[NRL_VERBOSE],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = 0", 0 == nrl_level_mask_ptr[NRL_DEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = 0",
                    0 == nrl_level_mask_ptr[NRL_VERBOSEDEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (12)");
  nrl_error(NRL_TEST, "NRL_ERROR should be present (12)");
  nrl_warning(NRL_TEST, "NRL_WARNING should be present (12)");
  nrl_info(NRL_TEST, "NRL_INFO should be present (12)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE should not be present (12)");
  nrl_debug(NRL_TEST, "NRL_DEBUG should not be present (12)");
  nrl_verbosedebug(NRL_TEST, "NRL_VERBOSEDEBUG should not be present (12)");
}

static void test_set_log_level_3_subsystems(void) {
  nr_status_t rv;

  /*
   * Don't set an overall level, only set 3 subsystems to different values.
   */
  rv = nrl_set_log_level("metrics=info,listener=verbose,daemon=verbosedebug");
  tlib_pass_if_true(
      "set level (metrics=info,listener=verbose,daemon=verbosedebug)",
      NR_SUCCESS == rv, "rv=%d", (int)rv);
  tlib_pass_if_true("mask[always] = ALL",
                    NRL_ALL_FLAGS == nrl_level_mask_ptr[NRL_ALWAYS],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ALWAYS]);
  tlib_pass_if_true("mask[error] = METRICS|LISTENER|DAEMON",
                    (NRL_METRICS | NRL_LISTENER | NRL_DAEMON)
                        == nrl_level_mask_ptr[NRL_ERROR],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_ERROR]);
  tlib_pass_if_true("mask[warning] = METRICS|LISTENER|DAEMON",
                    (NRL_METRICS | NRL_LISTENER | NRL_DAEMON)
                        == nrl_level_mask_ptr[NRL_WARNING],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_WARNING]);
  tlib_pass_if_true(
      "mask[info] = METRICS|LISTENER|DAEMON",
      (NRL_METRICS | NRL_LISTENER | NRL_DAEMON) == nrl_level_mask_ptr[NRL_INFO],
      "mask=0x%08x", nrl_level_mask_ptr[NRL_INFO]);
  tlib_pass_if_true(
      "mask[verbose] = LISTENER|DAEMON",
      (NRL_LISTENER | NRL_DAEMON) == nrl_level_mask_ptr[NRL_VERBOSE],
      "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSE]);
  tlib_pass_if_true("mask[debug] = DAEMON",
                    NRL_DAEMON == nrl_level_mask_ptr[NRL_DEBUG], "mask=0x%08x",
                    nrl_level_mask_ptr[NRL_DEBUG]);
  tlib_pass_if_true("mask[verbosedebug] = DAEMON",
                    NRL_DAEMON == nrl_level_mask_ptr[NRL_VERBOSEDEBUG],
                    "mask=0x%08x", nrl_level_mask_ptr[NRL_VERBOSEDEBUG]);

  nrl_always("NRL_ALWAYS should be present (13)");
  nrl_error(NRL_TEST, "NRL_ERROR(TEST) should not be present (13)");
  nrl_error(NRL_LISTENER, "NRL_ERROR(LISTENER) should be present (13)");
  nrl_error(NRL_DAEMON, "NRL_ERROR(DAEMON) should be present (13)");
  nrl_error(NRL_METRICS, "NRL_ERROR(METRICS) should be present (13)");
  nrl_warning(NRL_TEST, "NRL_WARNING(TEST) should not be present (13)");
  nrl_warning(NRL_LISTENER, "NRL_WARNING(LISTENER) should be present (13)");
  nrl_warning(NRL_DAEMON, "NRL_WARNING(DAEMON) should be present (13)");
  nrl_warning(NRL_METRICS, "NRL_WARNING(METRICS) should be present (13)");
  nrl_info(NRL_TEST, "NRL_INFO(TEST) should not be present (13)");
  nrl_info(NRL_LISTENER, "NRL_INFO(LISTENER) should be present (13)");
  nrl_info(NRL_METRICS, "NRL_INFO(METRICS) should be present (13)");
  nrl_info(NRL_DAEMON, "NRL_INFO(DAEMON) should be present (13)");
  nrl_verbose(NRL_TEST, "NRL_VERBOSE(TEST) should not be present (13)");
  nrl_verbose(NRL_LISTENER, "NRL_VERBOSE(LISTENER) should be present (13)");
  nrl_verbose(NRL_METRICS, "NRL_VERBOSE(METRICS) should not be present (13)");
  nrl_verbose(NRL_DAEMON, "NRL_VERBOSE(DAEMON) should be present (13)");
  nrl_debug(NRL_TEST, "NRL_DEBUG(TEST) should not be present (13)");
  nrl_debug(NRL_LISTENER, "NRL_DEBUG(LISTENER) should not be present (13)");
  nrl_debug(NRL_METRICS, "NRL_DEBUG(METRICS) should not be present (13)");
  nrl_debug(NRL_DAEMON, "NRL_DEBUG(DAEMON) should be present (13)");
  nrl_verbosedebug(NRL_TEST,
                   "NRL_VERBOSEDEBUG(TEST) should not be present (13)");
  nrl_verbosedebug(NRL_LISTENER,
                   "NRL_VERBOSEDEBUG(LISTENER) should not be present (13)");
  nrl_verbosedebug(NRL_METRICS,
                   "NRL_VERBOSEDEBUG(METRICS) should not be present (13)");
  nrl_verbosedebug(NRL_DAEMON,
                   "NRL_VERBOSEDEBUG(DAEMON) should be present (13)");
}

static void test_set_log_level(void) {
  test_set_log_level_simple();
  test_set_log_level_null();
  test_set_log_level_warning();
  test_set_log_level_info();
  test_set_log_level_verbose();
  test_set_log_level_debug();
  test_set_log_level_verbosedebug();
  test_set_log_level_bogus();
  test_set_log_level_overall();
  test_set_log_level_invalid_subsystem();
  test_set_log_level_3_subsystems();
}

static void test_vlog_helper(nrloglev_t level,
                             uint32_t subsystem,
                             const char* fmt,
                             ...) {
  va_list ap;

  va_start(ap, fmt);
  nrl_vlog(level, subsystem, fmt, ap);
  va_end(ap);
}

static void test_vlog(void) {
  nr_unlink("vlogtest.tmp");
  nrl_set_log_file("./vlogtest.tmp");
  nrl_set_log_level("warning");

  test_vlog_helper(NRL_ALWAYS, NRL_TEST, "%s", "NRL_ALWAYS");
  test_vlog_helper(NRL_ERROR, NRL_TEST, "%s", "NRL_ERROR");
  test_vlog_helper(NRL_WARNING, NRL_TEST, "%s", "NRL_WARNING");
  test_vlog_helper(NRL_INFO, NRL_TEST, "%s", "NRL_INFO");
  test_vlog_helper(NRL_VERBOSE, NRL_TEST, "%s", "NRL_VERBOSE");
  test_vlog_helper(NRL_DEBUG, NRL_TEST, "%s", "NRL_DEBUG");
  test_vlog_helper(NRL_VERBOSEDEBUG, NRL_TEST, "%s", "NRL_VERBOSEDEBUG");

  nrl_close_log_file();

  tlib_pass_if_not_diff("vlogtest.tmp", REFERENCE_DIR "/test_vlog.cmp",
                        cleanup_string, 0, 0);
}

static void test_format_timestamp(const char* msg,
                                  time_t utc_time,
                                  const char* expected_timestamp) {
  char buf[64];
  struct timeval tv;

  tv.tv_sec = utc_time;
  tv.tv_usec = 0;
  nr_memset(buf, 0, sizeof(buf));
  nrl_format_timestamp(buf, sizeof(buf), &tv);
  tlib_pass_if_str_equal(msg, expected_timestamp, buf);
}

static void test_timezones(void) {
  struct tm tm;
  time_t jan1_midnight_utc;
  time_t jul1_midnight_utc;
  char* saved_tz = getenv("TZ");

  if (saved_tz) {
    saved_tz = nr_strdup(saved_tz);
  }

  /*
   * The only portable way to create UTC timestamps is to force
   * the timezone to UTC, invoke mktime(3), and then restore
   * the local timezone.
   */

  setenv("TZ", "UTC", 1 /* overwrite */);
  tzset();

  nr_memset(&tm, 0, sizeof(tm));
  tm.tm_year = 2015 - 1900;
  tm.tm_mon = 0;
  tm.tm_mday = 1;
  jan1_midnight_utc = mktime(&tm);

  nr_memset(&tm, 0, sizeof(tm));
  tm.tm_year = 2015 - 1900;
  tm.tm_mon = 6;
  tm.tm_mday = 1;
  jul1_midnight_utc = mktime(&tm);

  /*
   * See http://en.wikipedia.org/wiki/List_of_tz_database_time_zones for
   * more test cases.
   */

  setenv("TZ", "America/Los_Angeles", 1 /* overwrite */);
  tzset();
  test_format_timestamp("integral timezone with negative offset",
                        jan1_midnight_utc, "2014-12-31 16:00:00.000 -0800");
  test_format_timestamp(
      "integral timezone with negative offset during daylight savings",
      jul1_midnight_utc, "2015-06-30 17:00:00.000 -0700");

  setenv("TZ", "America/St_Johns", 1 /* overwrite */);
  tzset();
  test_format_timestamp("half hour timezone with negative offset",
                        jan1_midnight_utc, "2014-12-31 20:30:00.000 -0330");
  test_format_timestamp(
      "half hour timezone with negative offset during daylight savings",
      jul1_midnight_utc, "2015-06-30 21:30:00.000 -0230");

  setenv("TZ", "Europe/Dublin", 1 /* overwrite */);
  tzset();
  test_format_timestamp("integral timezone with a positive offset",
                        jan1_midnight_utc, "2015-01-01 00:00:00.000 +0000");
  test_format_timestamp(
      "integral timezone with a positive offset during daylight savings",
      jul1_midnight_utc, "2015-07-01 01:00:00.000 +0100");

  setenv("TZ", "Pacific/Chatham", 1 /* overwrite */);
  tzset();
  test_format_timestamp("quarter hour timezone with a positive offset",
                        jan1_midnight_utc, "2015-01-01 13:45:00.000 +1345");
  test_format_timestamp(
      "quarter hour timezone with a positive offset during daylight savings",
      jul1_midnight_utc, "2015-07-01 12:45:00.000 +1245");

  setenv("TZ", "Pacific/Honolulu", 1 /* overwrite */);
  tzset();
  test_format_timestamp("timezone without daylight savings", jan1_midnight_utc,
                        "2014-12-31 14:00:00.000 -1000");
  test_format_timestamp("timezone without daylight savings", jul1_midnight_utc,
                        "2015-06-30 14:00:00.000 -1000");

  if (saved_tz) {
    setenv("TZ", saved_tz, 1 /* overwrite */);
    nr_free(saved_tz);
  } else {
    unsetenv("TZ");
  }
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  test_logging();

  /*
   * We're done with the tests. Last few tests are to compare the two log files
   * generated during the tests to ensure that they have the correct contents.
   * These are tested after both the main logging tests and the audit log have
   * run in order to check that there was no bleed-through from one file to the
   * other.
   */

#if HAVE_BACKTRACE
  tlib_pass_if_not_diff("logtest1.tmp", REFERENCE_DIR "/test_logging_1.cmp",
                        cleanup_string, 0, 0);
#else
  tlib_pass_if_not_diff("logtest1.tmp",
                        REFERENCE_DIR "/test_logging_1_no_backtrace.cmp",
                        cleanup_string, 0, 0);
#endif

  tlib_pass_if_not_diff("logtest2.tmp", REFERENCE_DIR "/test_logging_2.cmp",
                        cleanup_string, 0, 0);

  test_vlog();
  test_timezones();
}
