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
#if 0
  if (NULL == nr_php_find_class_method(zce, "getinstalledpackages")
      || NULL == nr_php_find_class_method(zce, "getversion")) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Composer\\InstalledVersions class found, but methods not found");
    return false;
  }
#else
  if (NULL == nr_php_find_class_method(zce, "getallrawdata")
      || NULL == nr_php_find_class_method(zce, "getrootpackage")) {
    nrl_verbosedebug(
        NRL_INSTRUMENT,
        "Composer\\InstalledVersions class found, but methods not found");
    return false;
  }
#endif

  return true;
}

static int nr_execute_handle_autoload_composer_init(const char* vendor_path) {
  char* code = NULL;
  zval retval;
  int result = -1;

  if (nr_execute_handle_autoload_composer_is_initialized()) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s: already initialized", __func__);
    return NR_SUCCESS;
  }

#if 0
  (void)vendor_path;
  code
      = nr_formatf(""
        "(function() {"
        "  try {"
        "      if (class_exists('Composer\\InstalledVersions')) {"
        "          if (method_exists('Composer\\InstalledVersions', "
        "             'getInstalledPackages') && method_exists('Composer\\InstalledVersions', "
        "             'getVersion')) {"
        "            return true;"
        "          } else {"
        "            return false;"
        "          }"
        "      } else {"
        "        return false;"
        "      }"
        "  } catch (Exception $e) {"
        "      return NULL;"
        "  }"
        "})();");
#else
  code = nr_formatf("include_once '%s/composer/InstalledVersions.php';",
                    vendor_path);
#endif

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
  int result = -1;

#if 0
  char* getpackagename
      = ""
        "(function() {"
        "  try {"
        "      return \\Composer\\InstalledVersions::getInstalledPackages();"
        "  } catch (Exception $e) {"
        "      return NULL;"
        "  }"
        "})();";

  char* getversion
      = ""
        "(function() {"
        "  try {"
        "      $v = \\Composer\\InstalledVersions::getPrettyVersion(\"%s\");"
        "      if ($v != null && strlen($v) > 0) {"
        "          $v = ltrim($v, 'v');"
        "      }"
        "      return $v;"
        "  } catch (Exception $e) {"
        "      return NULL;"
        "  }"
        "})();";
#else
    char* getallrawdata
        = ""
        "(function() {"
        "  try {"
        "    $root_package = \\Composer\\InstalledVersions::getRootPackage();"
        "    $packages = array();"
        "    foreach (\\Composer\\InstalledVersions::getAllRawData() as $installed) { "
        "      foreach ($installed['versions'] as $packageName => $packageData) {"
        "        if ($packageName == @$root_package['name']) {"
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
#endif

  if (NR_SUCCESS != nr_execute_handle_autoload_composer_init(vendor_path)) {
    nrl_debug(NRL_INSTRUMENT,
              "%s - unable to initialize Composer runtime API - package info "
              "unavailable",
              __func__);
    return;
  }

  nrl_verbosedebug(NRL_INSTRUMENT, "%s - Composer runtime API available",
                   __func__);

#if 0
  result = zend_eval_string(getpackagename, &retval,
                            "get installed packages by name" TSRMLS_CC);
  if (result == SUCCESS) {
    if (Z_TYPE(retval) == IS_ARRAY) {
      zval* value;
      char* buf;
      int result2;
      zval retval2;
      char* version = NULL;
      (void)version;
      ZEND_HASH_FOREACH_VAL(Z_ARRVAL(retval), value) {
        if (Z_TYPE_P(value) == IS_STRING) {
          buf = nr_formatf(getversion, Z_STRVAL_P(value));
          result2 = zend_eval_string(buf, &retval2,
                                     "retrieve version for packages");
          nr_free(buf);
          if (SUCCESS == result2) {
            if (nr_php_is_zval_valid_string(&retval2)) {
              version = Z_STRVAL(retval2);
            } else if (nr_php_is_zval_null(&retval2)) {
              nrl_verbose(NRL_INSTRUMENT,
                          "version was IS_NULL for package %s",
                          Z_STRVAL_P(value));
              version = NULL;
            }
          }
        }
        zval_dtor(&retval2);
        nrl_verbosedebug(NRL_INSTRUMENT, "package %s, version %s",
                         NRSAFESTR(Z_STRVAL_P(value)), NRSAFESTR(version));
        if (NULL != version) {
          if (NRINI(vulnerability_management_package_detection_enabled)) {
            nr_txn_add_php_package(NRPRG(txn), NRSAFESTR(Z_STRVAL_P(value)),
                                  NRSAFESTR(version));
          }
          nr_fw_support_add_package_supportability_metric(
              NRPRG(txn), NRSAFESTR(Z_STRVAL_P(value)), NRSAFESTR(version));
        }
      }
      ZEND_HASH_FOREACH_END();
    } else {
      zval_dtor(&retval);
      return;
    }
    zval_dtor(&retval);
  }
#else
  result = zend_eval_string(getallrawdata, &retval, "composer_getallrawdata.php");
  if (SUCCESS != result) {
    nrl_verbosedebug(NRL_INSTRUMENT, "%s - composer_getallrawdata.php failed", __func__);
    return;
  }
  if (IS_ARRAY == Z_TYPE(retval)) {
    zend_string* package_name = NULL;
    zval* package_version = NULL;
    ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL(retval), package_name, package_version) {
      if (NULL == package_name || NULL == package_version) {
        continue;
      }
      if (nr_php_is_zval_non_empty_string(package_version)) {
        nrl_verbosedebug(NRL_INSTRUMENT, "package %s, version %s",
                         NRSAFESTR(ZSTR_VAL(package_name)),
                         NRSAFESTR(Z_STRVAL_P(package_version)));
        if (NRINI(vulnerability_management_package_detection_enabled)) {
          nr_txn_add_php_package(NRPRG(txn), NRSAFESTR(ZSTR_VAL(package_name)),
                                NRSAFESTR(Z_STRVAL_P(package_version)));
        }
        nr_fw_support_add_package_supportability_metric(
            NRPRG(txn), NRSAFESTR(ZSTR_VAL(package_name)),
            NRSAFESTR(Z_STRVAL_P(package_version)));
      }
    }
    ZEND_HASH_FOREACH_END();
  } else {
    nrl_verbosedebug(NRL_INSTRUMENT,
                     "%s - installed packages is not an array", __func__);
  }
  zval_dtor(&retval);
#endif
}

static char* nr_execute_handle_autoload_composer_get_vendor_path(
    const char* filename) {
  char* vendor_path = NULL;  // result of dirname(filename)
  char* cp = NULL;

  // vendor_path = dirname(filename):
  // 1. copy filename to vendor_path
  vendor_path = nr_strdup(filename);
  // 2. // find last occurence of '/' in vendor_path
  cp = nr_strrchr(vendor_path, '/');
  // 3. replace '/' with '\0' to get the directory path
  *cp = '\0';

  return vendor_path;
}

static bool nr_execute_handle_autoload_composer_file_exists(
    const char* vendor_path,
    const char* filename) {
  char* composer_magic_file = NULL;  // vendor_path + filename
  bool file_exists = false;

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
    return;
  }

  if (!nr_execute_handle_autoload_composer_file_exists(vendor_path,
                                                       COMPOSER_MAGIC_FILE_2)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "'%s' not found in '%s'",
                     COMPOSER_MAGIC_FILE_2, vendor_path);
    return;
  }

  if (!nr_execute_handle_autoload_composer_file_exists(vendor_path,
                                                       COMPOSER_MAGIC_FILE_3)) {
    nrl_verbosedebug(NRL_FRAMEWORK, "'%s' not found in '%s'",
                     COMPOSER_MAGIC_FILE_3, vendor_path);
    return;
  }

  nrl_verbosedebug(NRL_FRAMEWORK, "detected composer");
  NRPRG(txn)->composer_info.composer_detected = true;
  nr_fw_support_add_library_supportability_metric(NRPRG(txn), "Composer");

  nr_execute_handle_autoload_composer_get_packages_information(vendor_path);
  nr_free(vendor_path);
}
