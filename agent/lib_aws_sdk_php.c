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

static void aws_sdk_php_version() {
  zval* zval_version = NULL;
  zend_class_entry* class_entry = NULL;
  char* version = NULL;

  /* Use the Aws\Sdk::VERSION to determine the version */
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

NR_PHP_WRAPPER(nr_aws) {
  (void)wraprec;
  NR_PHP_WRAPPER_CALL;

  aws_sdk_php_version();
}
NR_PHP_WRAPPER_END

void nr_aws_sdk_php_enable(TSRMLS_D) {
  /* This is the base of all other Clients */
  nr_php_wrap_user_function(NR_PSTR("Aws\\AwsClient::__construct"),
                            nr_aws TSRMLS_CC);
  /* The Sdk class is where version resides and has other useful features */
  nr_php_wrap_user_function(NR_PSTR("Aws\\Sdk::__construct"), nr_aws TSRMLS_CC);

  if (NRINI(vulnerability_management_package_detection_enabled)) {
    nr_txn_add_php_package(NRPRG(txn), PHP_PACKAGE_NAME,
                           PHP_PACKAGE_VERSION_UNKNOWN);
  }
}
