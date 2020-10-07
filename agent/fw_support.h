/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains functions which support framework code.
 */
#ifndef FW_SUPPORT_HDR
#define FW_SUPPORT_HDR

#include "php_user_instrument.h"

extern void nr_php_framework_add_supportability_metric(
    const char* framework_name,
    const char* name TSRMLS_DC);

#endif /* FW_SUPPORT_HDR */
