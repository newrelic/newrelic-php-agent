/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the php-ampqlib
 * https://github.com/php-amqplib/php-amqplib
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "lib_php_amqplib.h"

#define PHP_PACKAGE_NAME "php-amqplib/php-amqplib"

/*
 * Version detection will be called directly from PhpAmqpLib\\Package::VERSION
 * nr_php_amqplib_handle_version will automatically load the class if it isn't
 * loaded yet and then evaluate the string. To avoid the VERY unlikely but not
 * impossible fatal error if the file/class isn't loaded yet, we need to wrap
 * the call in a try/catch block and make it a lambda so that we avoid fatal
 * errors.
 */
void nr_php_amqplib_handle_version() {
  char* version = NULL;
  zval retval;
  int result = FAILURE;

  result = zend_eval_string(
      "(function() {"
      "     $nr_php_amqplib_version = '';"
      "     try {"
      "          $nr_php_amqplib_version = PhpAmqpLib\\Package::VERSION;"
      "     } catch (Throwable $e) {"
      "     }"
      "     return $nr_php_amqplib_version;"
      "})();",
      &retval, "Get nr_php_amqplib_version");

  /* See if we got a non-empty/non-null string for version. */
  if (SUCCESS == result) {
    if (nr_php_is_zval_non_empty_string(&retval)) {
      version = Z_STRVAL(retval);
    }
  }

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Add php package to transaction */
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }

  nr_txn_suggest_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                               version);

  zval_dtor(&retval);
}

void nr_php_amqplib_enable() {
  /*
   * Set the UNKNOWN package first, so it doesn't overwrite what we find with
   * nr_php_amqplib_handle_version.
   */
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }

  /* Extract the version for aws-sdk 3+ */
  nr_php_amqplib_handle_version();
}
