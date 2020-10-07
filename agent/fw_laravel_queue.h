/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains functions relating to Laravel queue instrumentation.
 */
#ifndef FW_LARAVEL_QUEUE_HDR
#define FW_LARAVEL_QUEUE_HDR

#include "php_newrelic.h"

extern void nr_laravel_queue_enable(TSRMLS_D);

#endif /* FW_LARAVEL_QUEUE_HDR */
