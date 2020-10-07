/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_POSTGRES_PRIVATE_HDR
#define NR_POSTGRES_PRIVATE_HDR

/*
 * Purpose : Retrieve the default host, port, or database for a Postgres
 *           connection.
 *
 * Returns : A newly allocated string. It's the responsibility of the caller
 *           to free this string after use.
 */
extern char* nr_postgres_default_host(void);
extern char* nr_postgres_default_port(void);
extern char* nr_postgres_default_database_name(void);

#endif /* NR_POSTGRES_PRIVATE_HDR */
