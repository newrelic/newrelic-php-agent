/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "tlib_php.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "php_agent.h"
#include "php_globals.h"
#include "fw_hooks.h"
#include "nr_agent.h"
#include "nr_app.h"
#include "nr_php_packages.h"
#include "util_syscalls.h"

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

static char* vendor_path = NULL;

/*
 * Every scenario in this file uses the same default ("PHP Application")
 * app, since none of them override newrelic.appname/license -- so the same
 * (app, tid) composer_map entry persists across scenarios unless reset.
 * Evicts it so each scenario starts from a genuinely fresh entry, matching
 * how a real worker thread's entry only ever starts fresh once, at thread
 * creation.
 */
static void reset_entry_between_scenarios(void) {
  nr_app_info_t info;
  nrapp_t* app;

  nr_memset(&info, 0, sizeof(info));
  nr_php_txn_populate_app_info_identity(&info, NULL, NULL TSRMLS_CC);

  app = nr_app_find_locked(nr_agent_applist, &info);
  if (NULL != app) {
    nr_app_tid_maps_evict(app, (uint64_t)nr_gettid());
    nrt_mutex_unlock(&app->app_lock);
  }

  nr_app_info_destroy_fields(&info);
}

/*
 * Confirms request start produced a live txn with a real, genuinely
 * fresh composer entry, asserting whichever precondition failed.
 * Returns the entry, or NULL if any assertion failed.
 */
static nr_composer_thread_entry_t* require_live_entry(TSRMLS_D) {
  nr_composer_thread_entry_t* entry;

  tlib_pass_if_not_null("request start produced a live txn", NRPRG(txn));
  if (NULL == NRPRG(txn)) {
    return NULL;
  }

  tlib_pass_if_not_null("txn has a real composer entry from RINIT's app",
                        NRPRG(txn)->composer_info.entry);
  if (NULL == NRPRG(txn)->composer_info.entry) {
    return NULL;
  }

  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("fresh entry starts UNSET",
                        (int)NR_COMPOSER_API_STATUS_UNSET, (int)entry->status);
  tlib_pass_if_null("fresh entry starts with no packages", entry->packages);
  tlib_pass_if_uint64_t_equal("fresh entry starts at epoch 0", 0, entry->epoch);

  return entry;
}

/*
 * Creates a throwaway vendor/composer/ directory with three empty magic
 * files so nr_composer_handle_autoload()'s file-exists gate passes. This is
 * created and removed at test time -- not a checked-in fixture.
 *
 * mkdtemp() (not a PID-based name) so a crashed prior run's leftover
 * directory can never collide with this one -- PIDs get reused, a random
 * suffix doesn't.
 */
static void create_vendor_dir(void) {
  char dir_template[] = "/tmp/nr_test_lib_composer_XXXXXX";
  char* composer_dir;
  static const char* const magic_files[]
      = {"autoload_real.php", "InstalledVersions.php", "installed.php"};
  size_t i;

  if (NULL == mkdtemp(dir_template)) {
    return;
  }
  vendor_path = nr_strdup(dir_template);

  composer_dir = nr_formatf("%s/composer", vendor_path);
  mkdir(composer_dir, 0755);

  for (i = 0; i < sizeof(magic_files) / sizeof(magic_files[0]); i++) {
    char* path = nr_formatf("%s/%s", composer_dir, magic_files[i]);
    FILE* f = fopen(path, "w");
    if (NULL != f) {
      fclose(f);
    }
    nr_free(path);
  }

  nr_free(composer_dir);
}

static void remove_vendor_dir(void) {
  static const char* const magic_files[]
      = {"autoload_real.php", "InstalledVersions.php", "installed.php"};
  size_t i;
  char* composer_dir;

  if (NULL == vendor_path) {
    return;
  }

  composer_dir = nr_formatf("%s/composer", vendor_path);
  for (i = 0; i < sizeof(magic_files) / sizeof(magic_files[0]); i++) {
    char* path = nr_formatf("%s/%s", composer_dir, magic_files[i]);
    unlink(path);
    nr_free(path);
  }
  rmdir(composer_dir);
  nr_free(composer_dir);

  rmdir(vendor_path);
  nr_free(vendor_path);
  vendor_path = NULL;
}

static char* autoload_filename(void) {
  return nr_formatf("%s/autoload.php", vendor_path);
}

/*
 * Every scenario below drives nr_composer_handle_autoload() through a real,
 * live PHP request rather than a hand-built nrtxn_t: tlib_php_request_start()
 * alone is enough to get a genuine NRPRG(txn)->composer_info.entry, because
 * the test engine's default INI already has a valid license and
 * tlib_php_engine_create() installs a synchronous appinfo stub, so RINIT's
 * normal app lookup/connect finishes instantly with no real daemon needed.
 */

/*
 * None of these scenarios call nr_composer_handle_autoload() with no live
 * txn (the FrankenPHP worker-bootstrap case, where the app hasn't yet been
 * acknowledged by the daemon) -- every scenario here runs through
 * tlib_php_request_start()'s live NRPRG(txn). That's adequate today only
 * because, once the (app, thread) entry is resolved, the function's write
 * logic never reads NRPRG(txn) again -- the with-txn and no-txn resolution
 * paths both hand it the same kind of entry pointer, and everything
 * downstream operates on that pointer alone, not on the txn. If the write
 * logic ever starts referencing NRPRG(txn) directly, this file's coverage
 * would no longer stand in for the no-txn case, and a real no-txn scenario
 * would need to be added.
 */

/*
 * The 6 scenarios below cover every combination of the entry's prior
 * status (never scanned, already collected, or a previous failure) and
 * this attempt's own result (success or failure). Each scenario number
 * matches the row number here:
 *
 * #  prior status   this result  packages          epoch      status after
 * 1  UNSET          success      NULL -> install   +1         COLLECTED
 * 2  UNSET          failure      stays NULL        unchanged  failure code
 * 3  COLLECTED      success      destroy, install  +1         COLLECTED
 * 4  COLLECTED      failure      untouched         unchanged  stays COLLECTED
 * 5  prior failure  success      NULL -> install   +1         COLLECTED
 * 6  prior failure  failure      stays NULL        unchanged  latest failure code
 */

/* Scenario 1: a fresh (never-scanned) entry, one successful scan. */
static void test_scenario_1_fresh_entry_success(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;

  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  /*
   * A minimal, real Composer\InstalledVersions stub. is_initialized()'s
   * class/method-existence check short-circuits before ever touching the
   * filesystem for the include, so the real scan logic (zend_eval_string
   * running the actual getallrawdata snippet) executes against this stub for
   * real -- no mocking of the function under test.
   */
  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {"
      "  public static function getAllRawData() {"
      "    return array(array('versions' => array("
      "      'vendor/package' => array('pretty_version' => 'v1.2.3'),"
      "    )));"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}" TSRMLS_CC);

  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);

  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("successful scan sets status to COLLECTED",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_not_null("successful scan installs packages", entry->packages);
  tlib_pass_if_uint64_t_equal("successful scan bumps epoch to 1", 1,
                             entry->epoch);
  {
    nr_php_package_t* package
        = (NULL == entry->packages)
              ? NULL
              : nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("the scanned package is present", package);
    tlib_pass_if_str_equal(
        "the scanned package's version is captured, 'v' prefix stripped",
        "1.2.3", NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("successful scan has exactly one package", 1,
                              nr_php_packages_count(entry->packages));
  }

end:
  tlib_php_request_end();
}

/*
 * Scenario 2: a fresh entry, one failed scan (INIT_FAILURE) -- no
 * Composer\InstalledVersions stub is ever defined, and the magic files on
 * disk are empty, so is_initialized() stays false even after the
 * include_once attempt. Asserts the entry stays untouched: packages/epoch
 * never written on a failure with no prior good data, and status records
 * the failure code (closing the once-per-thread gate for a
 * never-yet-successful entry).
 */
static void test_scenario_2_fresh_entry_init_failure(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);

  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("failed scan on fresh entry records INIT_FAILURE",
                        (int)NR_COMPOSER_API_STATUS_INIT_FAILURE,
                        (int)entry->status);
  tlib_pass_if_null("failed scan never installs packages", entry->packages);
  tlib_pass_if_uint64_t_equal("failed scan never bumps epoch", 0,
                             entry->epoch);

end:
  tlib_php_request_end();
}

/*
 * Scenario 3: an already-COLLECTED entry, then a second successful scan --
 * both this attempt's success and the resulting entry should simply reflect
 * the latest scan, same as today's unconditional-overwrite behavior on the
 * success path.
 */
static void test_scenario_3_collected_then_success(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {"
      "  public static $nr_test_calls = 0;"
      "  public static function getAllRawData() {"
      "    self::$nr_test_calls++;"
      "    if (self::$nr_test_calls === 1) {"
      "      return array(array('versions' => array("
      "        'vendor/package' => array('pretty_version' => 'v1.2.3'),"
      "      )));"
      "    }"
      "    return array(array('versions' => array("
      "      'vendor/other-package' => array('pretty_version' => 'v4.5.6'),"
      "    )));"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}" TSRMLS_CC);

  filename = autoload_filename();

  /* Call 1: fresh entry -> COLLECTED. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 1 collects successfully",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_uint64_t_equal("call 1 bumps epoch to 1", 1, entry->epoch);
  {
    nr_php_package_t* package
        = nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("call 1 has vendor/package", package);
    tlib_pass_if_str_equal("call 1's package version is captured", "1.2.3",
                          NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("call 1 has exactly one package", 1,
                              nr_php_packages_count(entry->packages));
  }

  /* Call 2: already COLLECTED -> a second success still replaces packages
   * and bumps epoch again. The stub returns a different package on this
   * call specifically so the assertions below can distinguish "replaced"
   * from "left alone". */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 2 stays COLLECTED",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_not_null("call 2 still has packages", entry->packages);
  tlib_pass_if_uint64_t_equal("call 2 bumps epoch to 2", 2, entry->epoch);
  tlib_pass_if_null(
      "call 1's package is gone after call 2's replacement",
      nr_php_packages_get_package(entry->packages, "vendor/package"));
  {
    nr_php_package_t* package = nr_php_packages_get_package(
        entry->packages, "vendor/other-package");
    tlib_pass_if_not_null("call 2's own package is present", package);
    tlib_pass_if_str_equal("call 2's package version is captured", "4.5.6",
                          NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("call 2 has exactly one package", 1,
                              nr_php_packages_count(entry->packages));
  }

  nr_free(filename);

end:
  tlib_php_request_end();
}

/*
 * Scenario 4: an already-COLLECTED entry, then a second scan that fails
 * with INVALID_RESULT. The failed second attempt must leave the good data
 * from call 1 completely untouched (packages, epoch, and status all
 * unchanged).
 *
 * Uses a stateful stub (a static call counter) rather than two separately
 * eval'd classes, since PHP won't allow redeclaring
 * Composer\InstalledVersions within the same request: call 1 returns real
 * data; call 2+ throws, which the getallrawdata snippet's own
 * catch (Throwable) turns into a non-array return (NULL), i.e.
 * INVALID_RESULT.
 */
static void test_scenario_4_collected_then_invalid_result(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;
  nr_php_packages_t* packages_after_call1;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {"
      "  public static $nr_test_calls = 0;"
      "  public static function getAllRawData() {"
      "    self::$nr_test_calls++;"
      "    if (self::$nr_test_calls === 1) {"
      "      return array(array('versions' => array("
      "        'vendor/package' => array('pretty_version' => 'v1.2.3'),"
      "      )));"
      "    }"
      "    throw new \\Exception('simulated Composer API failure');"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}" TSRMLS_CC);

  filename = autoload_filename();

  /* Call 1: fresh entry -> COLLECTED, real good data. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 1 collects successfully",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  {
    nr_php_package_t* package
        = nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("call 1 has vendor/package", package);
    tlib_pass_if_str_equal("call 1's package version is captured", "1.2.3",
                          NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("call 1 has exactly one package", 1,
                              nr_php_packages_count(entry->packages));
  }
  packages_after_call1 = entry->packages;

  /* Call 2: fails with INVALID_RESULT -- the bug-fix assertion: entry must
   * be completely unchanged from call 1, including the packages pointer
   * itself (never destroyed-and-replaced on a failed rescan). */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal(
      "call 2's failure does not change status away from COLLECTED",
      (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED, (int)entry->status);
  tlib_pass_if_true("call 2's failure leaves the same packages pointer",
                    entry->packages == packages_after_call1,
                    "entry->packages == packages_after_call1");
  tlib_pass_if_uint64_t_equal(
      "call 2's failure does not bump epoch past call 1's", 1, entry->epoch);
  {
    nr_php_package_t* package
        = (NULL == entry->packages)
              ? NULL
              : nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null(
        "the package from call 1 is still present after call 2's failure",
        package);
    tlib_pass_if_str_equal(
        "the package's version is unchanged after call 2's failure", "1.2.3",
        NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("call 2's failure leaves exactly one package",
                              1, nr_php_packages_count(entry->packages));
  }

  nr_free(filename);

end:
  tlib_php_request_end();
}

/*
 * Scenario 5: call 1 fails (INIT_FAILURE, no class defined yet), call 2
 * succeeds (the stub is eval'd for the first time between calls -- no
 * redeclare conflict, since call 1 never defined anything).
 *
 * This calls nr_composer_handle_autoload() directly, bypassing
 * agent/php_execute.c's per-thread gate, which would normally block a
 * second call once status is no longer UNSET -- this tests what the
 * write logic does if a second call happens anyway, not whether the
 * gate would allow one.
 */
static void test_scenario_5_failure_then_success(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  filename = autoload_filename();

  /* Call 1: no Composer\InstalledVersions defined yet -> INIT_FAILURE. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 1 fails with INIT_FAILURE",
                        (int)NR_COMPOSER_API_STATUS_INIT_FAILURE,
                        (int)entry->status);
  tlib_pass_if_null("call 1's failure leaves packages NULL", entry->packages);
  tlib_pass_if_uint64_t_equal("call 1's failure leaves epoch at 0", 0,
                             entry->epoch);

  /* Now define the stub for the first time -- nothing was ever declared by
   * call 1, so this is not a redeclare. */
  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {"
      "  public static function getAllRawData() {"
      "    return array(array('versions' => array("
      "      'vendor/package' => array('pretty_version' => 'v1.2.3'),"
      "    )));"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}" TSRMLS_CC);

  /* Call 2: succeeds now that the class exists. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 2 recovers to COLLECTED",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_uint64_t_equal("call 2 bumps epoch to 1", 1, entry->epoch);
  {
    nr_php_package_t* package
        = nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("call 2 has vendor/package", package);
    tlib_pass_if_str_equal("call 2's package version is captured", "1.2.3",
                          NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("call 2 has exactly one package", 1,
                              nr_php_packages_count(entry->packages));
  }

  nr_free(filename);

end:
  tlib_php_request_end();
}

/*
 * Scenario 6: repeated failure, same failure code both times. Uses a
 * malformed stub -- a Composer\InstalledVersions class missing the required
 * methods, defined once -- so is_initialized() permanently fails against it
 * (the class exists, so nr_php_find_class succeeds, but
 * nr_php_find_class_method fails every time, deterministically, with no
 * state needed).
 *
 * This calls nr_composer_handle_autoload() directly, bypassing
 * agent/php_execute.c's per-thread gate, which would normally block a
 * second call once status is no longer UNSET -- this tests what the
 * write logic does if a second call happens anyway, not whether the
 * gate would allow one.
 */
static void test_scenario_6_repeated_failure(TSRMLS_D) {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry(TSRMLS_C);
  if (NULL == entry) {
    goto end;
  }

  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {}" TSRMLS_CC);

  filename = autoload_filename();

  /* Call 1: class exists but is missing both required methods ->
   * INIT_FAILURE. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 1 fails with INIT_FAILURE",
                        (int)NR_COMPOSER_API_STATUS_INIT_FAILURE,
                        (int)entry->status);
  tlib_pass_if_null("call 1's failure leaves packages NULL", entry->packages);

  /* Call 2: nothing changed -- same deterministic failure again. */
  nr_composer_handle_autoload(filename);
  entry = NRPRG(txn)->composer_info.entry;
  tlib_pass_if_int_equal("call 2 fails with INIT_FAILURE again",
                        (int)NR_COMPOSER_API_STATUS_INIT_FAILURE,
                        (int)entry->status);
  tlib_pass_if_null("call 2's failure still leaves packages NULL",
                    entry->packages);
  tlib_pass_if_uint64_t_equal("repeated failure never bumps epoch", 0,
                             entry->epoch);

  nr_free(filename);

end:
  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  create_vendor_dir();

  test_scenario_1_fresh_entry_success(TSRMLS_C);
  test_scenario_2_fresh_entry_init_failure(TSRMLS_C);
  test_scenario_3_collected_then_success(TSRMLS_C);
  test_scenario_4_collected_then_invalid_result(TSRMLS_C);
  test_scenario_5_failure_then_success(TSRMLS_C);
  test_scenario_6_repeated_failure(TSRMLS_C);

  remove_vendor_dir();
  tlib_php_engine_destroy();
}
