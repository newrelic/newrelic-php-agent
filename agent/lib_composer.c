/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_txn.h"
#include "nr_version.h"
#include "php_globals.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_syscalls.h"

static bool nr_execute_handle_autoload_composer_is_initialized() {
  zend_class_entry* zce = NULL;

  if (NULL == (zce = nr_php_find_class("composer\\installedversions"))) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "Composer\\InstalledVersions class not found");
    return false;
  };

  // the class is found - there's hope!
  if (NULL == nr_php_find_class_method(zce, "getallrawdata")
      || NULL == nr_php_find_class_method(zce, "getrootpackage")) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Composer\\InstalledVersions class found, but methods not found");
    return false;
  }

  return true;
}

static int nr_execute_handle_autoload_composer_init(const char* vendor_path) {
  char* code = NULL;
  zval retval;
  int result = FAILURE;

  if (nr_execute_handle_autoload_composer_is_initialized()) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: already initialized", __func__);
    return NR_SUCCESS;
  }

  code = nr_formatf("include_once '%s/composer/InstalledVersions.php';",
                    vendor_path);

  result = zend_eval_string(code, &retval, "newrelic\\init_composer_api");
  if (result != SUCCESS) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: zend_eval_string(%s) failed, result=%d", __func__,
                     code, result);
    nr_free(code);
    return NR_FAILURE;
  }

  zval_dtor(&retval);
  nr_free(code);

  // Make sure runtime API is available after loading
  // Composer\\InstalledVersions class:
  if (!nr_execute_handle_autoload_composer_is_initialized()) {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s: unable to initialize Composer runtime API", __func__);
    return NR_FAILURE;
  }

  return NR_SUCCESS;
}

/*
 * Purpose : Find (never create) the nrapp_t for the current (app, thread)
 *           when no txn exists to supply one, by building a throwaway
 *           nr_app_info_t search key from process-global/SAPI-global state.
 *           Mirrors agent/php_txn.c:893,971-973's fallback logic for
 *           appname/license (order between the two independent
 *           appname/license computations doesn't matter — only that each
 *           resolves to the same value RINIT would have used), so this
 *           always resolves to the same nr_app_info_t identity fields
 *           RINIT already used to create/find the app — otherwise
 *           nr_app_match cannot match.
 *
 *           nr_app_find_locked() rejects the search key outright unless
 *           nr_app_info_valid() passes, which requires appname, license,
 *           environment, lang, version, and redirect_collector to all be
 *           non-NULL (axiom/nr_app.c). Of those, only license, appname,
 *           trace_observer_host, and trace_observer_port are actually
 *           compared by nr_app_match(); environment/lang/version/
 *           redirect_collector are set below purely to satisfy that
 *           non-NULL gate, mirroring agent/php_txn.c:979,983-984,986's
 *           equivalent assignments.
 *
 * Returns : The matching nrapp_t, with app->app_lock held (caller must
 *           unlock), or NULL if no match exists yet.
 */
static nrapp_t* nr_composer_find_app_no_txn(TSRMLS_D) {
  nr_app_info_t info;
  nrapp_t* app;
  char* appnames = NULL;
  char* raw_license = NULL;
  const char* lic_to_use;

  nr_memset(&info, 0, sizeof(info));

#ifdef ZTS
  if (nr_streq(sapi_module.name, "frankenphp")) {
    appnames = nr_php_get_server_global("NEW_RELIC_APP_NAME" TSRMLS_CC);
    raw_license = nr_php_get_server_global("NEW_RELIC_LICENSE_KEY" TSRMLS_CC);
  }
#endif

  /* appname fallback, ownership-equivalent to agent/php_txn.c:971-973 (that
   * code reassigns the pointer and strdup's once, later, unconditionally;
   * this does the strdup inline in the fallback branch instead — same end
   * state, different structure, since this function has no later
   * unconditional strdup point to defer to) */
  if ((NULL == appnames) || (0 == appnames[0])) {
    nr_free(appnames);
    appnames = nr_strdup(NRINI(appnames));
  }
  info.appname = appnames; /* ownership transferred; freed by
                              nr_app_info_destroy_fields below */

  /* license fallback, matching agent/php_txn.c:893 exactly (same helper) */
  lic_to_use = nr_php_use_license(raw_license TSRMLS_CC);
  info.license = (NULL != lic_to_use) ? nr_strdup(lic_to_use) : NULL;
  nr_free(raw_license);

  info.high_security = NR_PHP_PROCESS_GLOBALS(high_security);

  /* Non-NULL-only fields required by nr_app_info_valid(); see the doc
   * comment above. */
  info.environment = nro_copy(NR_PHP_PROCESS_GLOBALS(appenv));
  info.lang = nr_strdup("php");
  info.version = nr_strdup(nr_version());
  info.redirect_collector = nr_strdup(NR_PHP_PROCESS_GLOBALS(collector));

  if (NRINI(distributed_tracing_enabled)) {
    info.trace_observer_host = nr_strdup(NRINI(trace_observer_host));
  } else {
    info.trace_observer_host = nr_strdup("");
  }
  info.trace_observer_port = NRINI(trace_observer_port);

  app = nr_app_find_locked(nr_agent_applist, &info);

  nr_app_info_destroy_fields(&info);
  return app; /* still locked if non-NULL */
}

static nr_composer_api_status_t
nr_execute_handle_autoload_composer_get_packages_information(
    const char* vendor_path,
    nr_php_packages_t* packages_out) {
  zval retval;  // This is used as a return value for zend_eval_string.
                // It will only be set if the result of the eval is SUCCESS.
  int result = FAILURE;
  nr_composer_api_status_t api_status = NR_COMPOSER_API_STATUS_UNSET;

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(!NRINI(vulnerability_management_package_detection_enabled))) {
    // do nothing when collecting package information for vulnerability
    // management is disabled
    return NR_COMPOSER_API_STATUS_INVALID_USE;
  }

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(!NRINI(vulnerability_management_composer_api_enabled))) {
    // do nothing when use of composer to collect package info is disabled
    return NR_COMPOSER_API_STATUS_INVALID_USE;
  }

  // clang-format off
  char* getallrawdata
        = ""
        "(function() {"
        "  try {"
        "    $root_package = \\Composer\\InstalledVersions::getRootPackage();"
        "    $packages = array();"
        "    foreach (\\Composer\\InstalledVersions::getAllRawData() as $installed) { "
        "      foreach ($installed['versions'] as $packageName => $packageData) {"
        "        if (!is_string($packageName)) {"
        "          continue;"
        "        }"
        "        if (is_array($root_package) && array_key_exists('name', $root_package) && $packageName == $root_package['name']) {"
        "          continue;"
        "        }"
        "        if (!array_key_exists('pretty_version', $packageData)) {"
        "          continue;"
        "        }"
        "        $pretty_version = $packageData['pretty_version'];"
        "        if (is_string($pretty_version)) {"
        "          $packages[$packageName] = ltrim($pretty_version, 'v');"
        "        }"
        "      }"
        "    }"
        "    return $packages;"
        "  } catch (Throwable $e) {"
        "    return NULL;"
        "  }"
        "})();";
  // clang-format on

  if (NR_SUCCESS != nr_execute_handle_autoload_composer_init(vendor_path)) {
    nrl_debug(NRL_INSTRUMENT,
              "%s - unable to initialize Composer runtime API - package info "
              "unavailable",
              __func__);
    return NR_COMPOSER_API_STATUS_INIT_FAILURE;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "%s - Composer runtime API available",
                   __func__);

  result
      = zend_eval_string(getallrawdata, &retval, "composer_getallrawdata.php");
  if (SUCCESS != result) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s - composer_getallrawdata.php failed",
                     __func__);
    return NR_COMPOSER_API_STATUS_CALL_FAILURE;
  }

  if (IS_ARRAY == Z_TYPE(retval)) {
    zend_string* package_name = NULL;
    zval* package_version = NULL;
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL(retval), package_name,
                                  package_version) {
      if (NULL == package_name || NULL == package_version) {
        continue;
      }
      if (nr_php_is_zval_non_empty_string(package_version)) {
        nrl_verbosedebug(NRL_INSTRUMENT, "package %s, version %s",
                         NRSAFESTR(ZSTR_VAL(package_name)),
                         NRSAFESTR(Z_STRVAL_P(package_version)));
        nr_php_packages_add_package(
            packages_out,
            nr_php_package_create_with_source(ZSTR_VAL(package_name),
                                              Z_STRVAL_P(package_version),
                                              NR_PHP_PACKAGE_SOURCE_COMPOSER));
      }
    }
    ZEND_HASH_FOREACH_END();
    api_status = NR_COMPOSER_API_STATUS_PACKAGES_COLLECTED;
  } else {
    char strbuf[80];
    nr_format_zval_for_debug(&retval, strbuf, 0, sizeof(strbuf) - 1, 0);
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s - installed packages is: " NRP_FMT ", not an array",
                     __func__, NRP_ARGSTR(strbuf));
    api_status = NR_COMPOSER_API_STATUS_INVALID_RESULT;
  }
  zval_dtor(&retval);
  return api_status;
}

static char* nr_execute_handle_autoload_composer_get_vendor_path(
    const char* filename) {
  char* vendor_path = NULL;  // result of dirname(filename)
  char* cp = NULL;

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(NULL == filename)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s - filename is NULL", __func__);
    return NULL;
  }

  // vendor_path = dirname(filename):
  // 1. copy filename to vendor_path
  vendor_path = nr_strdup(filename);
  // 2. // find last occurence of '/' in vendor_path
  cp = nr_strrchr(vendor_path, '/');
  // 3. replace '/' with '\0' to get the directory path
  if (NULL != cp) {
    *cp = '\0';
  } else {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s - no '/' in filename '%s'", __func__,
                     filename);
  }

  return vendor_path;
}

static bool nr_execute_handle_autoload_composer_file_exists(
    const char* vendor_path,
    const char* filename) {
  char* composer_magic_file = NULL;  // vendor_path + filename
  bool file_exists = false;

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(NULL == vendor_path)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s - vendor_path is NULL", __func__);
    return false;
  }

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(NULL == filename)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s - filename is NULL", __func__);
    return false;
  }

  composer_magic_file = nr_formatf("%s/%s", vendor_path, filename);
  if (0 == nr_access(composer_magic_file, F_OK | R_OK)) {
    file_exists = true;
  }
  nr_free(composer_magic_file);
  return file_exists;
}

void nr_composer_handle_autoload(const char* filename) {
// Composer signature file"
#define COMPOSER_MAGIC_FILE_1 "composer/autoload_real.php"
#define COMPOSER_MAGIC_FILE_1_LEN (sizeof(COMPOSER_MAGIC_FILE_1) - 1)
// Composer runtime API files:
#define COMPOSER_MAGIC_FILE_2 "composer/InstalledVersions.php"
#define COMPOSER_MAGIC_FILE_2_LEN (sizeof(COMPOSER_MAGIC_FILE_2) - 1)
#define COMPOSER_MAGIC_FILE_3 "composer/installed.php"
#define COMPOSER_MAGIC_FILE_3_LEN (sizeof(COMPOSER_MAGIC_FILE_3) - 1)
  char* vendor_path = NULL;  // result of dirname(filename)
  nr_composer_thread_entry_t* entry = NULL;

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(NULL == filename)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "%s - filename is NULL", __func__);
    return;
  }

  vendor_path = nr_execute_handle_autoload_composer_get_vendor_path(filename);
  if (NULL == vendor_path) {
    nrl_verbosedebug(NRL_FRAMEWORK, "unable to get vendor path from '%s'",
                     filename);
    return;
  }

  if (!nr_execute_handle_autoload_composer_file_exists(vendor_path,
                                                       COMPOSER_MAGIC_FILE_1)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "'%s' not found in '%s'",
                     COMPOSER_MAGIC_FILE_1, vendor_path);
    goto leave;
  }

  if (!nr_execute_handle_autoload_composer_file_exists(vendor_path,
                                                       COMPOSER_MAGIC_FILE_2)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "'%s' not found in '%s'",
                     COMPOSER_MAGIC_FILE_2, vendor_path);
    goto leave;
  }

  if (!nr_execute_handle_autoload_composer_file_exists(vendor_path,
                                                       COMPOSER_MAGIC_FILE_3)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "'%s' not found in '%s'",
                     COMPOSER_MAGIC_FILE_3, vendor_path);
    goto leave;
  }

  nrl_verbosedebug(NRL_FRAMEWORK, "detected composer");
  if (NULL != NRPRG(txn)) {
    NRPRG(txn)->composer_info.composer_detected = true;
    nr_fw_support_add_library_supportability_metric(NRPRG(txn), "Composer");
  }

  /* Step 1: resolve the per-(app,thread) entry — via the txn's cached
   * pointer if one exists, or via a fresh find-only lookup if not. */
  if (NULL != NRPRG(txn)) {
    entry = NRPRG(txn)->composer_info.entry;
  } else {
    nrapp_t* app = nr_composer_find_app_no_txn(TSRMLS_C);
    if (NULL != app) {
      entry = nr_app_get_or_create_thread_composer_entry(app,
                                                         (uint64_t)nr_gettid());
      /* entry pointer is now safe to use lock-free after this point, same
       * as the with-txn case (see nr_app.h's per-map locking contract) —
       * unlock immediately rather than holding through the write below. */
      nrt_mutex_unlock(&app->app_lock);
    }
  }

  if (NULL == entry) {
    nrl_debug(NRL_FRAMEWORK,
              "%s - no (app, thread) entry available for composer scan write; "
              "skipping (expected-unreachable per design assumptions)",
              __func__);
    goto leave;
  }

  /* This write runs lock-free, on purpose: `entry` was resolved above
   * either from NRPRG(txn)->composer_info.entry (itself fetched lock-free
   * by this same thread at txn begin) or via a fresh lookup keyed by
   * (uint64_t)nr_gettid() in the no-txn branch above, and no thread other
   * than the one that owns an entry ever touches it -- the same
   * same-owning-thread-only invariant that already justifies
   * nr_txn_pull_composer_packages()/nr_txn_mark_composer_packages_sent()
   * being lock-free in axiom/nr_txn.c. See the doc comment on
   * nr_composer_thread_entry_t in axiom/nr_app.h for the full rationale. */
  {
    nr_php_packages_t* fresh_packages = nr_php_packages_create();
    nr_composer_api_status_t result
        = nr_execute_handle_autoload_composer_get_packages_information(
            vendor_path, fresh_packages);

    /* Steps 2-4: destroy-if-rescan, install fresh collection, bump epoch */
    if (NULL != entry->packages) {
      nr_php_packages_destroy(&entry->packages);
    }
    entry->packages = fresh_packages;
    entry->epoch += 1;
    entry->status = result;
  }
leave:
  nr_free(vendor_path);
}
