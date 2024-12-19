/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the AWS-SDK-PHP.
 * https://github.com/aws/aws-sdk-php
 */
#include "php_agent.h"
#include "php_call.h"
#include "php_hash.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "lib_aws_sdk_php.h"

#define PHP_PACKAGE_NAME "aws/aws-sdk-php"

/*
 * In a normal course of events, the following line will always work
 * zend_eval_string("Aws\\Sdk::VERSION;", &retval, "Get AWS Version")
 * By the time we have detected the existence of the aws-sdk-php and with
 * default composer project settings, it is callable even from
 * nr_aws_sdk_php_enable which will automatically load the class if it isn't
 * loaded yet and then evaluate the string. In the rare case that files
 * are not loaded via autoloader and/or have non-default composer classload
 * settings, if the class is not found, PHP 8.2+ will generate an
 * error whenever it cannot find a class which must be caught. Calling this
 * from nr_aws_sdk_php_enable would allow the sdk version value to be set only
 * once. To avoid the VERY unlikely but not impossible fatal error, we need to
 * take an extra step to wrap the call in a try/catch block. This means we need
 * to have an additionial zend_string_eval to get the result, but we avoid fatal
 * errors.
 */
void nr_lib_aws_sdk_php_handle_version() {
  char* version = NULL;
  zval retval;
  int result = FAILURE;

  result = zend_eval_string(
      "$nr_aws_sdk_version = '';"
      "try {"
      "    $nr_aws_sdk_version = Aws\\Sdk::VERSION;"
      "} catch(Throwable $_e) {"
      "}",
      NULL, "Set nr_aws_sdk_version");

  if (SUCCESS == result) {
    result = zend_eval_string("$nr_aws_sdk_version", &retval,
                              "Get nr_aws_sdk_version");

    /* See if we got a non-empty/non-null string for version. */
    if (SUCCESS == result) {
      if (nr_php_is_zval_non_empty_string(&retval)) {
        version = Z_STRVAL(retval);
      }
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

void nr_lib_aws_sdk_php_add_supportability_service_metric(
    const char* service_name) {
  /* total MAX metric name length per agent-specs */
  char buf[MAX_METRIC_NAME_LEN];
  char* cp = NULL;

  if (nr_strempty(service_name)) {
    return;
  }
  if (NULL == NRPRG(txn)) {
    return;
  }

  cp = buf;
  nr_strcpy(cp, PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX);
  cp += PHP_AWS_SDK_SERVICE_NAME_METRIC_PREFIX_LEN - 1;
  nr_strlcpy(cp, service_name, MAX_AWS_SERVICE_NAME_LEN);
  nrm_force_add(NRPRG(txn) ? NRTXN(unscoped_metrics) : 0, buf, 0);
}

/*
 * AwsClient::parseClass
 * This is called from the base AwsClient class for every client associated
 * with a service during client initialization.
 * parseClass already computes the service name for internal use, so we don't
 * need to store it, we just need to snag it from the return value as it goes
 * through the client initialization process.
 */
NR_PHP_WRAPPER(nr_create_aws_service_metric) {
  (void)wraprec;

  zval** ret_val_ptr = NULL;
  ret_val_ptr = NR_GET_RETURN_VALUE_PTR;

  NR_PHP_WRAPPER_CALL;

  if (NULL != ret_val_ptr && nr_php_is_zval_valid_array(*ret_val_ptr)) {
    /* obtain ret_val_ptr[0] which contains the service name */
    zval* service_name
        = nr_php_zend_hash_index_find(Z_ARRVAL_P(*ret_val_ptr), 0);
    if (nr_php_is_zval_non_empty_string(service_name)) {
      nr_lib_aws_sdk_php_add_supportability_service_metric(
          Z_STRVAL_P(service_name));
    }
  }
}
NR_PHP_WRAPPER_END

/*
 * The ideal file to begin immediate detection of the aws-sdk is:
 * aws-sdk-php/src/functions.php
 * Unfortunately, Php8.2+ and composer autoload leads to the
 * file being optimized directly and not loaded.
 *
 * Options considered:
 *
 * 1. for PHP8.2, and only optimizable libraries, when encountering autoload.php
 * files, ask the file what includes it added and check against only the
 * optimizable library. Small overhead incurred when encountering an autoload
 * file, but detects aws-sdk-php immediately before any sdk code executes
 * (changes needed for this are detailed in the original PR)
 * 2. use a file that gets called later and only when AwsClient.php file is
 * called. It's called later and we'll miss some instrumentation, but if we're
 * only ever going to be interested in Client calls anyway, maybe that's ok?
 * Doesn't detect Sdk.php (optimized out) so when customers only use that or
 * when they use it first, we will not instrument it. This only detects when a
 * Client is called to use a service so potentially misses out on other
 * instrumentation and misses out when customers use the aws-sdk-php but use
 * non-SDK way to interact with the service (possibly with redis/memcached).
 * This way is definitely the least complex and lowest overhead and less
 * complexity means lower risk as well.
 * 3. Directly add the wrappers to the hash map. With potentially 50ish clients
 * to wrap, this will add overhead to every hash map lookup. Currently
 * implemented option is 2, use the AwsClient.php as this is our main focus.
 * This means until a call to an Aws/AwsClient function,
 * all calls including aws\sdk calls are ignored.
 *
 * Version detection will be called directly from Aws\Sdk.php
 */
void nr_aws_sdk_php_enable() {
  /*
   * Set the UNKNOWN package first, so it doesn't overwrite what we find with
   * nr_lib_aws_sdk_php_handle_version.
   */
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }

  /* Extract the version for aws-sdk 3+ */
  nr_lib_aws_sdk_php_handle_version();

  /* Called when initializing all Clients */
  nr_php_wrap_user_function(NR_PSTR("Aws\\AwsClient::parseClass"),
                            nr_create_aws_service_metric);
}
