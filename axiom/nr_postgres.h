/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_POSTGRES_HDR
#define NR_POSTGRES_HDR

/*
 * Purpose : Determine datastore instance information from the connection string
 *           provided to the Postgres driver.
 *
 * Params  : 1. The connection string
 *           2. A return value for the host
 *           3. A return value for the port_path_or_id
 *           4. A return value for the database_name
 *
 * Note    : It's the responsibility of the caller to free the returned strings
 *           after use.
 */
extern void nr_postgres_parse_conn_info(const char* conn_info,
                                        char** host,
                                        char** port_path_or_id,
                                        char** database_name);

#endif /* NR_POSTGRES_HDR */
