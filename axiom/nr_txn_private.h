/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_TXN_PRIVATE_HDR
#define NR_TXN_PRIVATE_HDR

#include "nr_txn.h"
#include "util_random.h"
#include "util_time.h"

extern char* nr_txn_create_guid(nr_random_t* rnd);
extern void nr_txn_handle_total_time(nrtxn_t* txn,
                                     nrtime_t total_time,
                                     void* userdata);
extern void nr_txn_create_rollup_metrics(nrtxn_t* txn);
extern void nr_txn_create_queue_metric(nrtxn_t* txn);
extern void nr_txn_create_duration_metrics(nrtxn_t* txn,
                                           nrtime_t duration,
                                           nrtime_t total_time);
extern void nr_txn_create_error_metrics(nrtxn_t* txn, const char* txnname);
extern void nr_txn_create_apdex_metrics(nrtxn_t* txn, nrtime_t duration);
extern void nr_txn_add_error_attributes(nrtxn_t* txn);
extern void nr_txn_record_custom_event_internal(nrtxn_t* txn,
                                                const char* type,
                                                const nrobj_t* params,
                                                nrtime_t now);

/* These sample options are provided for tests. */
extern const nrtxnopt_t nr_txn_test_options;

/*
 * Purpose : Adds CAT intrinsics to the passed nrobj_t.
 */
extern void nr_txn_add_cat_intrinsics(const nrtxn_t* txn, nrobj_t* intrinsics);

/*
 * Purpose : Adds distributed tracing intrinsics to the passed nrobj_t.
 */
extern void nr_txn_add_distributed_tracing_intrinsics(const nrtxn_t* txn,
                                                      nrobj_t* intrinsics);

/*
 * Purpose : Add an alternative path hash to the list maintained in the
 *           transaction.
 *
 * Params  : 1. The transaction.
 *           2. The (possibly) new path hash.
 */
extern void nr_txn_add_alternate_path_hash(nrtxn_t* txn, const char* path_hash);

/*
 * Purpose : Generate and return the current path hash for a transaction.
 *
 * Params  : 1. The transaction.
 *
 * Returns : The new path hash, which the caller is responsible for freeing.
 *
 * Note    : The key difference between this function and nr_txn_get_path_hash
 *           is that nr_txn_get_path_hash will also add the generated hash to
 *           the list of alternate path hashes, whereas this function only
 *           generates the hash but doesn't record it.
 */
extern char* nr_txn_current_path_hash(const nrtxn_t* txn);

/*
 * Purpose : Return the alternative path hashes in the form expected by the
 *           New Relic backend.
 *
 * Params  : 1. The transaction.
 *
 * Returns : A newly allocated string containing the alternative path hashes,
 *           sorted and comma separated.
 */
extern char* nr_txn_get_alternate_path_hashes(const nrtxn_t* txn);

/*
 * Purpose : Set the GUID for the given transaction.
 *
 * Params  : 1. The transaction.
 *           2. The GUID. This may be NULL to remove the GUID.
 *
 * Notes   : This function is intended for internal testing use only.
 */
extern void nr_txn_set_guid(nrtxn_t* txn, const char* guid);

/*
 * Purpose : Add a pattern to the list of files that will be matched on for
 *           transaction file naming.
 */
extern void nr_txn_add_file_naming_pattern(nrtxn_t* txn,
                                           const char* user_pattern);

/*
 * Purpose : Free all transaction fields.  This is provided as a helper function
 *           for tests where the transaction is a local stack variable.
 */
extern void nr_txn_destroy_fields(nrtxn_t* txn);

extern nrobj_t* nr_txn_event_intrinsics(const nrtxn_t* txn);

#endif /* NR_TXN_PRIVATE_HDR */
