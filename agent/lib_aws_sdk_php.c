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
 * settings, if the class is not found, PHP 8.2+ will generate a fatal
 * unrecoverable uncatchable error error whenever it cannot find a class. While
 * calling this from nr_aws_sdk_php_enable would have been great and would allow
 * the sdk version value to be set only once, to avoid the very unlikely but not
 * impossible fatal error, this will be called from the
 * "Aws\\ClientResolver::_apply_user_agent" wrapper which GUARANTEES that
 * aws/sdk exists and is already loaded.
 *
 *
 * Additionally given that aws-sdk-php is currently detected from the
 * AwsClient.php file, this method will always be called when a client is
 * created unlike Sdk::construct which doesn't show with PHP 8.2+.
 *
 * Using Aws/Sdk::__construct for version is currently nonviable as it is
 * unreliable as a version determiner.
 * Having separate functionality to extract from Aws/Sdk::__construct
 * is both not required and is redundant and causes additional overhead and
 * so only one function is needed to extract version.
 *
 * Aws\\ClientResolver::_apply_user_agent a reliable function as it is
 * always called on client initialization since it is key to populating
 * the request headers, and it loads Sdk by default.
 *
 * Concerns about future/past proofing to the checking prioritized the following
 * implementation vs using the eval method.
 */
extern void lib_aws_sdk_php_handle_version() {
  zval* zval_version = NULL;
  zend_class_entry* class_entry = NULL;
  char* version = NULL;

  class_entry = nr_php_find_class("aws\\sdk");
  zval_version = nr_php_get_class_constant(class_entry, "VERSION");

  if (nr_php_is_zval_non_empty_string(zval_version)) {
    version = Z_STRVAL_P(zval_version);
  }
  if (NRINI(vulnerability_management_package_detection_enabled)) {
    /* Add php package to transaction */
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME, version);
  }
  nr_fw_support_add_package_supportability_metric(NRPRG(txn), PHP_PACKAGE_NAME,
                                                  version);
}

extern void lib_aws_sdk_php_add_supportability_metric(const char* metric_name) {
  int MAX_LEN = 512;
  char buf[MAX_LEN];
  char DELIM = '_';
  char* mod_string = buf;

  if (NULL == metric_name || '\0' == metric_name[0]) {
    return;
  }
  if (NULL == NRPRG(txn)) {
    return;
  }

  /* First, proceed past any leading backslashes */
  const char* begin = metric_name;
  while ('\\' == *begin) {
    begin++;
  }

  /* If nothing is left in the string, return. */
  if (NULL == begin || '\0' == begin[0]) {
    return;
  }

  buf[0] = '\0';

  snprintf(buf, MAX_LEN, "%s%s", PHP_AWS_CLASS_PREFIX, begin);

  /* Replace backslashes */
  char* p = nr_strchr(mod_string, '\\');
  /* If it's not the end or NULL replace with delimiter*/
  while (NULL != p) {
    if (*(p + 1) == '\0') {
      /* We're at the end */
      *p = '\0';
      p = NULL;
    } else {
      /* Somewhere in the middle, let's replace */
      *p = DELIM;
      p = nr_strchr(p + 1, '\\');
    }
  }

  nrm_force_add(NRPRG(txn) ? NRTXN(unscoped_metrics) : 0, mod_string, 0);
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

  nr_php_wrap_user_function(NR_PSTR("Aws\\S3\\S3Client::__construct"),
                            nr_aws_create_metric);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
