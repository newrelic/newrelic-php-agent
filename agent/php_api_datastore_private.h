/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_API_DATASTORE_INTERNAL_HDR
#define PHP_API_DATASTORE_INTERNAL_HDR

#include "nr_datastore_instance.h"
#include "nr_txn.h"
#include "util_object.h"

#include <stdbool.h>

typedef struct {
  const char* key;
  zend_uchar final_type;
  bool required;
  const char* default_value;
} nr_php_api_datastore_segment_key_validators_t;

static const nr_php_api_datastore_segment_key_validators_t
    datastore_validators[]
    = {
        {"product", IS_STRING, true, NULL},
        {"collection", IS_STRING, false, "other"},
        {"operation", IS_STRING, false, "other"},
        {"host", IS_STRING, false, NULL},
        {"portPathOrId", IS_STRING, false, NULL},
        {"databaseName", IS_STRING, false, NULL},
        {"query", IS_STRING, false, NULL},
        {"inputQueryLabel", IS_STRING, false, NULL},
        {"inputQuery", IS_STRING, false, NULL},
};
static const size_t num_datastore_validators
    = sizeof(datastore_validators)
      / sizeof(nr_php_api_datastore_segment_key_validators_t);

/*
 * Purpose : Create datastore instance metadata from parameters.
 *
 * Params  : 1. The params containing the database name, the host
 *              and the port path or ID.
 *
 * Returns : A pointer to the datastore instance structure.
 */
extern nr_datastore_instance_t*
nr_php_api_datastore_create_instance_from_params(zval* params);

/*
 * Purpose : Validate the given parameters.
 *
 * Params  : 1. The params to be validated.
 *
 * Returns : A zval pointer to the validated parameters, or NULL if invalid.
 */
extern zval* nr_php_api_datastore_validate(const HashTable* params);

#endif /* PHP_API_DATASTORE_INTERNAL_HDR */
