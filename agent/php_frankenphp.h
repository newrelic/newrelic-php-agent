/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_FRANKENPHP_H
#define PHP_FRANKENPHP_H

#include "php_includes.h"

#ifdef ZTS

extern void nr_php_frankenphp_handle_request(INTERNAL_FUNCTION_PARAMETERS);

#endif

#endif
