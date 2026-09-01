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
  nr_php_txn_populate_app_info_identity(&info, NULL, NULL);

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
static nr_composer_thread_entry_t* require_live_entry() {
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
static void test_scenario_1_fresh_entry_success() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  tlib_php_request_start();

  entry = require_live_entry();
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
      "}");

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
static void test_scenario_2_fresh_entry_init_failure() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
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
static void test_scenario_3_collected_then_success() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
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
      "}");

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
static void test_scenario_4_collected_then_invalid_result() {
  char* filename;
  nr_composer_thread_entry_t* entry;
  nr_php_packages_t* packages_after_call1;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
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
      "}");

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
static void test_scenario_5_failure_then_success() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
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
      "}");

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
static void test_scenario_6_repeated_failure() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
  if (NULL == entry) {
    goto end;
  }

  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {}");

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

/*
 * The 4 scenarios below cover a different route into the same entry:
 * newrelic_ignore_transaction() and newrelic_set_appname()'s default
 * (no $xmit) discard, both of which drive nr_php_txn_end()'s ignoretxn
 * branch and thus nr_txn_discard_composer_packages(). Each API is tested
 * against both halves of that function's behavior: destroying a scan that
 * never got pulled/sent, and leaving an already-sent scan alone.
 *
 * #   API                   original entry before   packages after   status after
 * 7   ignore_transaction    new, unsent             destroyed        UNSET
 * 8   set_appname           new, unsent             destroyed        UNSET
 * 9   ignore_transaction    already sent            unchanged        COLLECTED
 * 10  set_appname           already sent            unchanged        COLLECTED
 *
 * (Rows 8 and 10 also create a second, new entry via set_appname's app
 * switch -- separately verified in each scenario to start fresh: UNSET, no
 * packages, epoch 0. This table describes only the original entry's fate,
 * not the new one's.)
 *
 * All four scenarios keep dereferencing their saved `entry` pointer after
 * calling tlib_php_request_end() (which destroys the current txn) or after
 * newrelic_set_appname() swaps NRPRG(txn) to a new one. That's safe because
 * the entry lives on the app's composer_map, keyed by (app, tid) -- not on
 * the txn object -- so it outlives whichever txn happened to be looking at
 * it when the scan ran.
 */

static const char* const one_package_stub
    = "namespace Composer;"
      "class InstalledVersions {"
      "  public static function getAllRawData() {"
      "    return array(array('versions' => array("
      "      'vendor/package' => array('pretty_version' => 'v1.2.3'),"
      "    )));"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}";

/*
 * Scenario 7: a scan happens, then newrelic_ignore_transaction() is called
 * before the request ends. Ignoring sets status.ignore before
 * nr_php_txn_end's ignoretxn snapshot is taken, so the normal pull/send path
 * is skipped entirely -- the scan is genuinely unsent when RSHUTDOWN runs
 * the discard.
 */
static void test_ignore_transaction_discards_unsent_scan() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
  if (NULL == entry) {
    tlib_php_request_end();
    return;
  }

  /* Precondition: scan, then confirm it produced new, unsent data. */
  tlib_php_request_eval(one_package_stub);
  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);

  tlib_pass_if_int_equal("scan collects successfully before ignoring",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_not_null("scan installs packages before ignoring",
                        entry->packages);
  tlib_pass_if_uint64_t_equal("scan bumps epoch to 1 before ignoring", 1,
                             entry->epoch);
  tlib_pass_if_uint64_t_equal(
      "scan's data is still unsent before ignoring (epoch != "
      "last_sent_epoch)",
      0, entry->last_sent_epoch);

  /* Action under test: ignore the transaction, then confirm the discard. */
  tlib_php_request_eval("newrelic_ignore_transaction();");
  tlib_php_request_end();

  tlib_pass_if_int_equal(
      "ignore_transaction resets status to UNSET when scan was unsent",
      (int)NR_COMPOSER_API_STATUS_UNSET, (int)entry->status);
  tlib_pass_if_null(
      "ignore_transaction destroys packages when scan was unsent",
      entry->packages);
  tlib_pass_if_uint64_t_equal(
      "ignore_transaction still advances last_sent_epoch to match epoch", 1,
      entry->last_sent_epoch);
}

/*
 * Scenario 8: same as 7, but via newrelic_set_appname()'s default discard.
 * That call fires nr_php_txn_end synchronously, before nr_php_txn_begin
 * swaps NRPRG(txn) to the new app -- so the discard is already visible on
 * the saved entry pointer by the time the eval call returns.
 */
static void test_set_appname_discards_unsent_scan() {
  char* filename;
  nr_composer_thread_entry_t* entry;

  reset_entry_between_scenarios();
  tlib_php_request_start();

  entry = require_live_entry();
  if (NULL == entry) {
    tlib_php_request_end();
    return;
  }

  /* Precondition: scan, then confirm it produced new, unsent data. */
  tlib_php_request_eval(one_package_stub);
  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);

  tlib_pass_if_int_equal("scan collects successfully before switching apps",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_not_null("scan installs packages before switching apps",
                        entry->packages);
  tlib_pass_if_uint64_t_equal("scan bumps epoch to 1 before switching apps",
                             1, entry->epoch);
  tlib_pass_if_uint64_t_equal(
      "scan's data is still unsent before switching apps (epoch != "
      "last_sent_epoch)",
      0, entry->last_sent_epoch);

  /* Action under test: switch apps, then confirm the discard. */
  tlib_php_request_eval(
      "newrelic_set_appname('SoakScratchAppUnsent');");

  /*
   * Confirms the switch landed on a genuinely different, genuinely fresh
   * entry -- not just a different pointer that happens to carry stale
   * state through some other path. This is a sanity check on the switch
   * itself, not on the discard behavior under test below.
   */
  tlib_pass_if_true(
      "set_appname moves NRPRG(txn) to a different composer entry",
      NRPRG(txn)->composer_info.entry != entry,
      "NRPRG(txn)->composer_info.entry != entry");
  tlib_pass_if_int_equal("new app's entry starts fresh (UNSET)",
                        (int)NR_COMPOSER_API_STATUS_UNSET,
                        (int)NRPRG(txn)->composer_info.entry->status);
  tlib_pass_if_null("new app's entry starts fresh (no packages)",
                    NRPRG(txn)->composer_info.entry->packages);
  tlib_pass_if_uint64_t_equal("new app's entry starts fresh (epoch 0)", 0,
                             NRPRG(txn)->composer_info.entry->epoch);

  tlib_pass_if_int_equal(
      "set_appname resets the old app's status to UNSET when scan was "
      "unsent",
      (int)NR_COMPOSER_API_STATUS_UNSET, (int)entry->status);
  tlib_pass_if_null(
      "set_appname destroys the old app's packages when scan was unsent",
      entry->packages);
  tlib_pass_if_uint64_t_equal(
      "set_appname still advances the old app's last_sent_epoch to match "
      "epoch",
      1, entry->last_sent_epoch);

  tlib_php_request_end();
}

/*
 * Scenario 9: a scan happens and is allowed to complete a normal (non-
 * ignored) request, so the ordinary pull/mark-sent path in nr_php_txn_end
 * catches it up (last_sent_epoch == epoch). A second request on the same
 * thread then calls newrelic_ignore_transaction() with nothing new to
 * discard -- the already-sent data must be left completely alone.
 */
static void test_ignore_transaction_preserves_already_sent_scan() {
  char* filename;
  nr_composer_thread_entry_t* entry;
  nr_php_packages_t* packages_after_first_send;

  reset_entry_between_scenarios();

  /* Request 1: plain scan, plain (non-ignored) end -- pull + mark-sent run
   * normally. */
  tlib_php_request_start();
  entry = require_live_entry();
  if (NULL == entry) {
    tlib_php_request_end();
    return;
  }

  tlib_php_request_eval(one_package_stub);
  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);
  tlib_php_request_end();

  tlib_pass_if_uint64_t_equal("first request's scan bumps epoch to 1", 1,
                             entry->epoch);
  tlib_pass_if_uint64_t_equal(
      "first request's normal end marks it fully sent", 1,
      entry->last_sent_epoch);
  packages_after_first_send = entry->packages;
  tlib_pass_if_not_null("packages survive the first request's normal end",
                        packages_after_first_send);

  /* Request 2: same thread, same (default) app -- nothing new has scanned,
   * so ignoring this request must be a no-op on the entry. */
  tlib_php_request_start();
  tlib_pass_if_true("second request reuses the same composer entry",
                    NRPRG(txn)->composer_info.entry == entry,
                    "NRPRG(txn)->composer_info.entry == entry");

  tlib_php_request_eval("newrelic_ignore_transaction();");
  tlib_php_request_end();

  tlib_pass_if_true(
      "already-sent packages pointer is untouched by a later discard",
      entry->packages == packages_after_first_send,
      "entry->packages == packages_after_first_send");
  {
    nr_php_package_t* package
        = nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("already-sent package is still present", package);
    tlib_pass_if_str_equal(
        "already-sent package version is unchanged", "1.2.3",
        NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("already-sent packages count is unchanged", 1,
                              nr_php_packages_count(entry->packages));
  }
  tlib_pass_if_int_equal("already-sent status stays COLLECTED",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_uint64_t_equal("already-sent epoch is unchanged", 1,
                             entry->epoch);
  tlib_pass_if_uint64_t_equal("already-sent last_sent_epoch is unchanged", 1,
                             entry->last_sent_epoch);
}

/*
 * Scenario 10: same setup as 9, but the second request calls
 * newrelic_set_appname() instead of newrelic_ignore_transaction(). Nothing
 * new has scanned since the first request's normal send, so switching apps
 * must leave the original app's already-sent entry alone.
 */
static void test_set_appname_preserves_already_sent_scan() {
  char* filename;
  nr_composer_thread_entry_t* entry;
  nr_php_packages_t* packages_after_first_send;

  reset_entry_between_scenarios();

  /* Request 1: plain scan, plain (non-ignored) end. */
  tlib_php_request_start();
  entry = require_live_entry();
  if (NULL == entry) {
    tlib_php_request_end();
    return;
  }

  tlib_php_request_eval(one_package_stub);
  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);
  tlib_php_request_end();

  tlib_pass_if_uint64_t_equal("first request's scan bumps epoch to 1", 1,
                             entry->epoch);
  tlib_pass_if_uint64_t_equal(
      "first request's normal end marks it fully sent", 1,
      entry->last_sent_epoch);
  packages_after_first_send = entry->packages;

  /* Request 2: same thread, same (default) app at start -- switch away via
   * set_appname with nothing new pending. */
  tlib_php_request_start();
  tlib_pass_if_true(
      "second request reuses the same composer entry before switching",
      NRPRG(txn)->composer_info.entry == entry,
      "NRPRG(txn)->composer_info.entry == entry");

  tlib_php_request_eval(
      "newrelic_set_appname('SoakScratchAppSent');");

  /*
   * Confirms the switch landed on a genuinely different, genuinely fresh
   * entry -- not just a different pointer that happens to carry stale
   * state through some other path. This is a sanity check on the switch
   * itself, not on the preserve behavior under test below.
   */
  tlib_pass_if_true(
      "set_appname moves NRPRG(txn) to a different composer entry",
      NRPRG(txn)->composer_info.entry != entry,
      "NRPRG(txn)->composer_info.entry != entry");
  tlib_pass_if_int_equal("new app's entry starts fresh (UNSET)",
                        (int)NR_COMPOSER_API_STATUS_UNSET,
                        (int)NRPRG(txn)->composer_info.entry->status);
  tlib_pass_if_null("new app's entry starts fresh (no packages)",
                    NRPRG(txn)->composer_info.entry->packages);
  tlib_pass_if_uint64_t_equal("new app's entry starts fresh (epoch 0)", 0,
                             NRPRG(txn)->composer_info.entry->epoch);

  /*
   * Scan the new app too, using a distinct package name -- not just to
   * confirm the bare act of switching leaves the original entry alone
   * (already covered), but to positively confirm that real activity on
   * the new app afterward doesn't bleed back into it either. A distinct
   * package name means any accidental cross-app bleed-through would be
   * unmistakable in the assertions below, not just a stale-looking match.
   */
  tlib_php_request_eval(
      "namespace Composer;"
      "class InstalledVersions {"
      "  public static function getAllRawData() {"
      "    return array(array('versions' => array("
      "      'vendor/new-app-package' => array('pretty_version' => "
      "'v9.9.9'),"
      "    )));"
      "  }"
      "  public static function getRootPackage() {"
      "    return array('name' => 'root/package');"
      "  }"
      "}");
  filename = autoload_filename();
  nr_composer_handle_autoload(filename);
  nr_free(filename);

  tlib_pass_if_int_equal("new app's own scan succeeds independently",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)NRPRG(txn)->composer_info.entry->status);
  tlib_pass_if_null(
      "new app's scan doesn't leak into the original entry's packages",
      nr_php_packages_get_package(entry->packages, "vendor/new-app-package"));

  tlib_pass_if_true(
      "already-sent packages pointer is untouched by set_appname's discard",
      entry->packages == packages_after_first_send,
      "entry->packages == packages_after_first_send");
  {
    nr_php_package_t* package
        = nr_php_packages_get_package(entry->packages, "vendor/package");
    tlib_pass_if_not_null("already-sent package is still present", package);
    tlib_pass_if_str_equal(
        "already-sent package version is unchanged", "1.2.3",
        NULL == package ? NULL : package->package_version);
    tlib_pass_if_size_t_equal("already-sent packages count is unchanged", 1,
                              nr_php_packages_count(entry->packages));
  }
  tlib_pass_if_int_equal("already-sent status stays COLLECTED",
                        (int)NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED,
                        (int)entry->status);
  tlib_pass_if_uint64_t_equal("already-sent epoch is unchanged", 1,
                             entry->epoch);
  tlib_pass_if_uint64_t_equal("already-sent last_sent_epoch is unchanged", 1,
                             entry->last_sent_epoch);

  tlib_php_request_end();
}

void test_main(void* p NRUNUSED) {
  tlib_php_engine_create("");
  create_vendor_dir();

  test_scenario_1_fresh_entry_success();
  test_scenario_2_fresh_entry_init_failure();
  test_scenario_3_collected_then_success();
  test_scenario_4_collected_then_invalid_result();
  test_scenario_5_failure_then_success();
  test_scenario_6_repeated_failure();

  test_ignore_transaction_discards_unsent_scan();
  test_set_appname_discards_unsent_scan();
  test_ignore_transaction_preserves_already_sent_scan();
  test_set_appname_preserves_already_sent_scan();

  remove_vendor_dir();
  tlib_php_engine_destroy();
}
