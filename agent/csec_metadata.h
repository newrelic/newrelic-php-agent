/*
 * Copyright 2024 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CSEC_METADATA_H
#define CSEC_METADATA_H

typedef enum {
    NR_PHP_CSEC_METADATA_HIGH_SECURITY = 1,
    NR_PHP_CSEC_METADATA_ENTITY_NAME,
    NR_PHP_CSEC_METADATA_ENTITY_TYPE,
    NR_PHP_CSEC_METADATA_ENTITY_GUID,
    NR_PHP_CSEC_METADATA_HOST_NAME,
    NR_PHP_CSEC_METADATA_AGENT_RUN_ID,
    NR_PHP_CSEC_METADATA_ACCOUNT_ID,
    NR_PHP_CSEC_METADATA_LICENSE,
    NR_PHP_CSEC_METADATA_PLICENSE
} nr_php_csec_metadata_key_t;

/*
 * Purpose : Copy requested app meta data into allocated *value.
 *           The caller is responsible for freeing the memory
 *           allocated.
 *
 * Params  : Pointer to a nr_php_csec_metadata_t structure
 *
 * Returns : 0 for success
 *          -1 for invalid input
 *          -2 for invalid internal state
 *          -3 for inability to allocate memory
 *          -4 for invalid metadata key
 *          -5 for inability to retrieve metadata value
 */
extern int nr_php_csec_get_metadata(const nr_php_csec_metadata_key_t k, void** value);
typedef int (*nr_php_csec_get_metadata_t)(const nr_php_csec_metadata_key_t k, void** value);
#define NR_PHP_CSEC_GET_METADATA "nr_php_csec_get_metadata"
#endif
