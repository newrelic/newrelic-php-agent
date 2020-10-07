/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Functions relating to instrumenting Doctrine ORM or DBAL 2.
 */
#ifndef LIB_DOCTRINE2_HDR
#define LIB_DOCTRINE2_HDR

#include "nr_slowsqls.h"

/*
 * Purpose : If we're currently executing a user-generated DQL query, return
 *           it.
 *
 * Returns : A newly allocated nr_slowsqls_labelled_query_t if successful, NULL
 *           otherwise. It is the caller's responsibility to call nr_free.
 */
extern nr_slowsqls_labelled_query_t* nr_doctrine2_lookup_input_query(TSRMLS_D);

extern void nr_doctrine2_enable(TSRMLS_D);

#endif /* LIB_DOCTRINE2_HDR */
