/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_SEGMENT_DATASTORE_PRIVATE_H
#define NR_SEGMENT_DATASTORE_PRIVATE_H

#include "nr_txn.h"

/*
 * Purpose : Determine if the given node duration is long enough to trigger a
 *           slow SQL node.
 *
 * Params  : 1. The transaction pointer.
 *           2. The node duration.
 *
 * Returns : Non-zero if the node would trigger a node; zero otherwise.
 */
bool nr_segment_datastore_stack_worthy(const nrtxn_t* txn, nrtime_t duration);

/*
 * Purpose : Extract the operation ('insert', 'update', etc) and the table name.
 *
 * Params  : 1. The current transaction.
 *           2. Pointer to location to return operation string.  This string
 *              is constant and must not be freed.
 *           3. The NULL-terminated SQL.
 *           4. The modify table name callback.
 *
 * Returns : The table name, or NULL if it could not be extracted. The table
 *           name is owned by the caller, and must be freed once it is no
 *           longer required.
 */
char* nr_segment_sql_get_operation_and_table(
    const nrtxn_t* txn,
    const char** operation_ptr,
    const char* sql,
    nr_modify_table_name_fn_t modify_table_name_fn);

#endif /* NR_SEGMENT_DATASTORE_PRIVATE_H */
