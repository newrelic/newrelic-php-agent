/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting AWS-SDK-PHP.
 */
#ifndef LIB_AWS_SDK_PHP_HDR
#define LIB_AWS_SDK_PHP_HDR

extern void nr_aws_sdk_php_enable();
extern void lib_aws_sdk_php_handle_version();
extern void lib_aws_sdk_php_add_supportability_metric(const char* metric_name);

#endif /* LIB_AWS_SDK_PHP_HDR */
