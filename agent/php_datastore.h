/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PHP_DATASTORE_HDR
#define PHP_DATASTORE_HDR

#include "nr_datastore.h"
#include "nr_explain.h"

/*
 * Purpose : Wrap nr_segment_datastore_end() to create an SQL node.
 *
 * Params  : 1. The segment.
 *           2. The SQL query.
 *           3. The size of the SQL query.
 *           4. The explain plan JSON, if any.
 *           5. The datastore type.
 *           6. The datastore instance, if any.
 */
extern void nr_php_txn_end_segment_sql(nr_segment_t** segment_ptr,
                                       const char* sql,
                                       int sqllen,
                                       const nr_explain_plan_t* plan,
                                       nr_datastore_t datastore,
                                       nr_datastore_instance_t* instance
                                           TSRMLS_DC);

/*
 * Purpose : Make a character string from a connection object or resource. If
 *           the zval is NULL, the key will include the extension name.
 *
 * Params  : 1. The mysql connection object
 *           2. The extension name
 *
 * Returns : A pointer to the newly created key, which the caller should free.
 */
extern char* nr_php_datastore_make_key(const zval* conn, const char* extension);

/*
 * Purpose : Determine whether the datastore connections hashmap contains a
 *           value for a given key
 *
 * Params  : 1. A datastore connections key
 *
 * Returns : Non-zero if the hashmap contains a value for the key, otherwise
 *           zero.
 */
extern int nr_php_datastore_has_conn(const char* key TSRMLS_DC);

/*
 * Purpose : Store a datastore instance metadata in the datastore connections
 *           hashmap.
 *
 * Params  : 1. A datastore connections key
 *           2. A pointer to the instance to store
 */
extern void nr_php_datastore_instance_save(const char* key,
                                           nr_datastore_instance_t* instance
                                               TSRMLS_DC);

/*
 * Purpose : Retrieve datastore instance metadata for a datastore connection.
 *
 * Params  : 1. A datastore connections key
 *
 * Returns : A pointer to the datastore instance structure or NULL on error.
 */
extern nr_datastore_instance_t* nr_php_datastore_instance_retrieve(
    const char* key TSRMLS_DC);

/*
 * Purpose : Remove datastore instance metadata for a datastore connection.
 *
 * Params  : 1. A datastore connections key
 */
extern void nr_php_datastore_instance_remove(const char* key TSRMLS_DC);

#endif /* PHP_DATASTORE_HDR */
