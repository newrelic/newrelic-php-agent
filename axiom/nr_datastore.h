/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DATASTORE_HDR
#define NR_DATASTORE_HDR

#include <stddef.h>

/*
 * This is the list of datastore types supported.
 * This exists to ensure that the datastore types included in metrics are
 * consistent with other agents.  That is why nr_segment_datastore_end() takes
 * a nr_datastore_t rather than a string.
 *
 * When an item is added to this list, it needs to be added to
 * datastore_mappings in nr_datastore_private.h as well.
 */
typedef enum {
  NR_DATASTORE_OTHER = 0,
  NR_DATASTORE_MONGODB,
  NR_DATASTORE_MEMCACHE,
  NR_DATASTORE_MYSQL,
  NR_DATASTORE_REDIS,
  NR_DATASTORE_MSSQL,
  NR_DATASTORE_ORACLE,
  NR_DATASTORE_POSTGRES,
  NR_DATASTORE_SQLITE,
  NR_DATASTORE_FIREBIRD,
  NR_DATASTORE_ODBC,
  NR_DATASTORE_SYBASE,
  NR_DATASTORE_INFORMIX,
  NR_DATASTORE_PDO,
  NR_DATASTORE_MUST_BE_LAST
} nr_datastore_t;

/*
 * Purpose : Return a string representation of the datastore type.
 */
extern const char* nr_datastore_as_string(nr_datastore_t ds);

/*
 * Purpose : Return the datastore type for the given string.
 */
extern nr_datastore_t nr_datastore_from_string(const char* str);

/*
 * Purpose : Test if the given datastore type is a SQL datastore.
 */
extern int nr_datastore_is_sql(nr_datastore_t ds);

#endif /* NR_DATASTORE_HDR */
