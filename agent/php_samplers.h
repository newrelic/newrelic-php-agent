/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file contains periodic system samplers.
 */
#ifndef PHP_SAMPLERS_HDR
#define PHP_SAMPLERS_HDR

/*
 * Purpose : Initialize the system samplers.
 */
extern void nr_php_initialize_samplers(void);

/*
 * Purpose : Sample system resources and store results so that system usage can
 *           later be properly calculated.
 */
extern void nr_php_resource_usage_sampler_start(TSRMLS_D);

/*
 * Purpose : Sample system resources and add the results to the transaction's
 *           metric table.
 */
extern void nr_php_resource_usage_sampler_end(TSRMLS_D);

#endif /* PHP_SAMPLERS_HDR */
