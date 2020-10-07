/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains functions relating to Laravel instrumentation.
 */
#ifndef FW_LARAVEL_HDR
#define FW_LARAVEL_HDR

/*
 * Purpose : Register the newrelic\Laravel\AfterFilter class used for Laravel
 *           transaction naming.
 */
extern void nr_laravel_minit(TSRMLS_D);

/*
 * The class entry for the newrelic\Laravel\AfterFilter object.
 */
extern zend_class_entry* nr_laravel_afterfilter_ce;

#endif /* FW_LARAVEL_HDR */
