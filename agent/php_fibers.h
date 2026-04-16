/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NEWRELIC_PHP_AGENT_PHP_FIBERS_H
#define NEWRELIC_PHP_AGENT_PHP_FIBERS_H

#if ZEND_MODULE_API_NO >= ZEND_8_1_X_API_NO

#include "Zend/zend_fibers.h"

#endif  // PHP 8.1+
#endif  // NEWRELIC_PHP_AGENT_PHP_FIBERS_H
