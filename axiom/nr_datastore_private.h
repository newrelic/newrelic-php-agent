/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DATASTORE_PRIVATE_HDR
#define NR_DATASTORE_PRIVATE_HDR

typedef struct {
  nr_datastore_t datastore;
  const char* str;
  const char* lowercase;
  int is_sql;
} nr_datastore_mapping_t;

/*
 * These strings must conform to the New Relic specifications to ensure agent
 * consistency.
 */
static const nr_datastore_mapping_t datastore_mappings[] = {
    {NR_DATASTORE_OTHER, NULL, NULL, 0},
    {NR_DATASTORE_MONGODB, "MongoDB", "mongodb", 0},
    {NR_DATASTORE_MEMCACHE, "Memcached", "memcached", 0},
    {NR_DATASTORE_MYSQL, "MySQL", "mysql", 1},
    {NR_DATASTORE_REDIS, "Redis", "redis", 0},
    {NR_DATASTORE_MSSQL, "MSSQL", "mssql", 1},
    {NR_DATASTORE_ORACLE, "Oracle", "oracle", 1},
    {NR_DATASTORE_POSTGRES, "Postgres", "postgres", 1},
    {NR_DATASTORE_SQLITE, "SQLite", "sqlite", 1},
    {NR_DATASTORE_FIREBIRD, "Firebird", "firebird", 1},
    {NR_DATASTORE_ODBC, "ODBC", "odbc", 0},
    {NR_DATASTORE_SYBASE, "Sybase", "sybase", 1},
    {NR_DATASTORE_INFORMIX, "Informix", "informix", 1},
    {NR_DATASTORE_PDO, "PDO", "pdo", 0},
    {NR_DATASTORE_DYNAMODB, "DynamoDB", "dynamodb", 0},
    {NR_DATASTORE_MUST_BE_LAST, NULL, NULL, 0},
};
static size_t datastore_mappings_len
    = sizeof(datastore_mappings) / sizeof(nr_datastore_mapping_t);

#endif /* NR_DATASTORE_PRIVATE_HDR */
