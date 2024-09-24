/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "php_agent.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "nr_txn.h"
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

static void nr_execute_handle_autoload_composer_get_packages_information(
    const char* vendor_path) {
  zval retval;  // This is used as a return value for zend_eval_string.
                // It will only be set if the result of the eval is SUCCESS.
  int result = FAILURE;

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(!NRINI(vulnerability_management_package_detection_enabled))) {
    // do nothing when collecting package information for vulnerability
    // management is disabled
    return;
  }

  // nrunlikely because this should alredy be ensured by the caller
  if (nrunlikely(!NRINI(vulnerability_management_composer_api_enabled))) {
    // do nothing when use of composer to collect package info is disabled
    return;
  }

  // clang-format off
  const char* getallrawdata
        = ""
        "(function() {"
        "  try {"
        "    $root_package = \\Composer\\InstalledVersions::getRootPackage();"
        "    $packages = array();"
        "    foreach (\\Composer\\InstalledVersions::getAllRawData() as $installed) { "
        "      foreach ($installed['versions'] as $packageName => $packageData) {"
        "        if (is_array($root_package) && array_key_exists('name', $root_package) && $packageName == $root_package['name']) {"
        "          continue;"
        "        }"
        "        if (isset($packageData['pretty_version'])) {"
        "          $packages[$packageName] = ltrim($packageData['pretty_version'], 'v');"
        "        }"
        "      }"
        "    }"
        "    return $packages;"
        "  } catch (Exception $e) {"
        "    return NULL;"
        "  }"
        "})();";
  // clang-format on

  if (NR_SUCCESS != nr_execute_handle_autoload_composer_init(vendor_path)) {
    nrl_debug(NRL_INSTRUMENT,
              "%s - unable to initialize Composer runtime API - package info "
              "unavailable",
              __func__);
    return;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "%s - Composer runtime API available",
                   __func__);

  result
      = zend_eval_string(getallrawdata, &retval, "composer_getallrawdata.php");
  if (SUCCESS != result) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s - composer_getallrawdata.php failed",
                     __func__);
    return;
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
        nr_txn_add_php_package_from_source(
            NRPRG(txn), NRSAFESTR(ZSTR_VAL(package_name)),
            NRSAFESTR(Z_STRVAL_P(package_version)),
            NR_PHP_PACKAGE_SOURCE_COMPOSER);
      }
    }
    ZEND_HASH_FOREACH_END();
  } else {
    char strbuf[80];
    nr_format_zval_for_debug(&retval, strbuf, 0, sizeof(strbuf) - 1, 0);
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s - installed packages is: " NRP_FMT ", not an array",
                     __func__, NRP_ARGSTR(strbuf));
  }
  zval_dtor(&retval);
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
  NRPRG(txn)->composer_info.composer_detected = true;
  nr_fw_support_add_library_supportability_metric(NRPRG(txn), "Composer");

  nr_execute_handle_autoload_composer_get_packages_information(vendor_path);
leave:
  nr_free(vendor_path);
}
