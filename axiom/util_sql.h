/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains sql helper routines.
 */
#ifndef UTIL_SQL_HDR
#define UTIL_SQL_HDR

#include <stdint.h>

/*
 * Purpose : Obfuscate the given SQL.
 *
 * Params  : 1. The raw SQL.
 *
 * Returns : An allocated obfuscated version of the SQL where digits and string
 *           contents replaced with ? or NULL on error.
 *
 * Notes   : Enclosing string quotations are removed during obfuscation. For
 *           example "foo bar" => ?
 *           This function is idempotent.
 */
extern char* nr_sql_obfuscate(const char* raw);

/*
 * Purpose : Normalize the given obfuscated SQL.
 *
 * Params  : 1. The obfuscated SQL.
 *
 * Returns : An allocated normalization of obfuscated SQL, or NULL on error.
 *
 * Notes   : "Normalization" means turning multiple groups of (?,?...) into a
 *           single (?). For example, turns "SELECT * FROM foo WHERE COL1 IN
 *           (?,?,?,?,?,?,?)" into "SELECT * FROM foo WHERE COL1 IN (?)".
 *           This is only done for IN clauses, as per the SQL tracer spec.
 */
extern char* nr_sql_normalize(const char* obfuscated_sql);

/*
 * Purpose : Compute a hash of a normalized, obfuscated SQL string.
 *
 * Params  : 1. The obfuscated SQL.
 *
 * Returns : The hash, as created by nr_mkhash(), or 0 on error.
 */
extern uint32_t nr_sql_normalized_id(const char* obfuscated_sql);

/*
 * Purpose : Get the operation ('insert', 'update', etc) and the table name.
 *
 * Params  : 1. The NUL-terminated SQL.
 *           2. Pointer to location to return operation string.  This string
 *              is constant and must not be freed.
 *           3. Pointer to location to return the table name.  This string is
 *              allocated and must be freed by the caller.
 */
extern void nr_sql_get_operation_and_table(const char* sql,
                                           const char** operation_ptr,
                                           char** table_ptr,
                                           int show_sql_parsing);

#endif /* UTIL_SQL_HDR */
