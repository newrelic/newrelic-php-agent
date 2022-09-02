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

/*
 * Purpose: Add `Supportability/library/{library}/detected` unscoped metric
 *
 * Params  : 1. Transaction object
 *           2. Library name
 *
 */
extern void nr_fw_support_add_library_supportability_metric(
    nrtxn_t* txn,
    const char* library_name);

/*
 * Purpose: Add `Supportability/Logging/PHP/{library}/{enabled|disabled}`
 * unscoped metric
 *
 * Params  : 1. Transaction object
 *           2. Library name
 *           3. Instrumentation status
 *
 */
extern void nr_fw_support_add_logging_supportability_metric(
    nrtxn_t* txn,
    const char* library_name,
    const bool is_enabled);

#endif /* FW_SUPPORT_HDR */
