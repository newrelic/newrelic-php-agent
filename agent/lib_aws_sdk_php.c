/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Functions relating to instrumenting the AWS-SDK-PHP.
 * https://github.com/aws/aws-sdk-php
 */
#include "php_agent.h"
#include "php_wrapper.h"
#include "fw_hooks.h"
#include "fw_support.h"
#include "util_logging.h"
#include "php_call.h"
#include "lib_aws_sdk_php.h"

#define PHP_PACKAGE_NAME "aws/aws-sdk-php"
#define PHP_AWS_CLASS_PREFIX "Supportability/library/aws/aws-sdk-php/"

/*
 * In a normal course of events, the following line will always work
 * zend_eval_string("Aws\\Sdk::VERSION;", &retval, "Get AWS Version")
 * By the time we have detected the existence of the aws-sdk-php and with
 * default composer profject settings, it callable even from
 * nr_aws_sdk_php_enable which will automatically load the class if it isn't
 * loaded yet and then evaluate the string. However, in the rare case that files
 * are not loaded via autoloader and/or have non-default composer classload
 * settings, if the class is not found, PHP will generate a fatal unrecoverable
 * uncatchable error. While calling this from nr_aws_sdk_php_enable would have
 * been great and would allow the sdk version value to be set only once, to
 * avoid the very unlikely but not impossible fatal error, this will be called
 * from the "Aws\\ClientResolver::_apply_user_agent" wrapper which
 * GUARANTEES that aws/sdk exists and is already loaded.
 *
 *
 * Additionally given that aws-sdk-php is currently detected from the
 * AwsClient.php file, this method will always be called when a client is
 * created. Having separate functionality to extract from Aws/Sdk::__constructor
 * is both not required and is redundant and causes additional overhead.
 */
extern void lib_aws_sdk_php_handle_version() {
  char* version = NULL;
  zval retval;
  int result = FAILURE;

  result = zend_eval_string("Aws\\Sdk::VERSION;", &retval, "Get AWS Version");

  // Add php package to transaction
  if (SUCCESS == result) {
    if (nr_php_is_zval_non_empty_string(&retval)) {
      version = Z_STRVAL(retval);
    }
    zval_dtor(&retval);
  }

  /* Use the Aws\Sdk::VERSION to determine the version */
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Add php package to transaction */
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }
  nr_fw_support_add_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                                  version);
}

extern void lib_aws_sdk_php_add_supportability_metric(const char* metric_name) {
  char buf[512];
  nrobj_t* names = NULL;

  if (NULL == metric_name) {
    return;
  }
  if (NULL == NRPRG(txn)) {
    return;
  }

  /* Strip out the `\\` characters and normalize */
  names = nr_strsplit(metric_name, "\\", 0 /* discard empty */);

  buf[0] = '\0';
  snprintf(buf, sizeof(buf), "%s%s", PHP_AWS_CLASS_PREFIX,
           nro_get_array_string(names, 1, NULL));
  for (int i = 2, n = nro_getsize(names); i <= n; i++) {
    const char* name = nro_get_array_string(names, i, NULL);
    if (NULL != name) {
      snprintf(buf, sizeof(buf), "%s_%s", buf, name);
    }
  }
  nro_delete(names);

  nrm_force_add(NRPRG(txn) ? NRTXN(unscoped_metrics) : 0, buf, 0);
}

/*
 * Any wrapped function pointing here will automatically have it's class
 * name created as a supportability metric.
 */
NR_PHP_WRAPPER(nr_aws_create_metric) {
  if (NULL != wraprec) {
    lib_aws_sdk_php_add_supportability_metric(wraprec->classname);
  }
  NR_PHP_WRAPPER_CALL;
}
NR_PHP_WRAPPER_END

NR_PHP_WRAPPER(nr_aws_version) {
  (void)wraprec;

  lib_aws_sdk_php_handle_version();
  NR_PHP_WRAPPER_CALL;
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
 * 2. use a file that gets called later and only when AwsClient.php file file is
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
 * all calls including aws\sdk calls are ignored.  Version detection will be
 * tied to Aws/ClientResolver::_apply_user_agent which is ALWAYS called when
 * dealing with aws clients.  It will not be computed from
 * Aws/Sdk::__constructor which would at best be duplicate info and worst would
 * never be ignored until a client is called.
 */
void nr_aws_sdk_php_enable() {
  /* This is will guarantee we can extract the version. */
  nr_php_wrap_user_function(NR_PSTR("Aws\\ClientResolver::_apply_user_agent"),
                            nr_aws_version);
  /* This is the base of all other Clients */
  nr_php_wrap_user_function(NR_PSTR("Aws\\AwsClient::__construct"),
                            nr_aws_create_metric);
  /* The Sdk class */
  nr_php_wrap_user_function(NR_PSTR("Aws\\Sdk::__construct"),
                            nr_aws_create_metric);
}
