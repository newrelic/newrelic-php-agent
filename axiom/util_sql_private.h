/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains internal data structures for the sql interface
 */
#ifndef UTIL_SQL_PRIVATE_HDR
#define UTIL_SQL_PRIVATE_HDR

extern const char* nr_sql_whitespace_comment_prefix(const char* sql,
                                                    int show_sql_parsing);

#endif /* UTIL_SQL_PRIVATE_HDR */
