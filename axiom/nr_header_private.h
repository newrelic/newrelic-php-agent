/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains internal functions for the management of cross process
 * headers.
 */
#ifndef NR_HEADER_PRIVATE_HDR
#define NR_HEADER_PRIVATE_HDR

#include "nr_txn.h"

extern char* nr_header_encode(const nrtxn_t* txn, const char* string);
extern void nr_header_outbound_response_decoded(nrtxn_t* txn,
                                                const char* decoded_response,
                                                char** external_id_ptr,
                                                char** external_txnname_ptr,
                                                char** external_guid_ptr);
extern char* nr_header_decode(const nrtxn_t* txn, const char* encoded_string);

extern nr_status_t nr_header_validate_encoded_string(
    const char* encoded_string);
extern nr_status_t nr_header_validate_decoded_id(const nrtxn_t* txn,
                                                 const char* decoded_id);

extern char* nr_header_inbound_response_internal(nrtxn_t* txn,
                                                 int content_length);

/*
 * Purpose : Extracts the account ID from the cross process ID.
 *
 * Params  : 1. A cross process ID (generally a decoded X-NewRelic-ID header).
 *
 * Returns : An account ID, or -1 on error.
 */
extern int64_t nr_header_account_id_from_cross_process_id(
    const char* cross_process_id);

/*
 * Purpose : Decodes an inbound synthetics header.
 *
 * Params  : 1. The current transaction.
 *           2. The X-NewRelic-Synthetics header value.
 *
 * Returns : The decoded header value, ready for nr_synthetics_create. The
 *           caller will need to free this.
 */
extern char* nr_header_inbound_synthetics(const nrtxn_t* txn,
                                          const char* x_newrelic_synthetics);

extern nr_status_t nr_header_process_x_newrelic_transaction(
    nrtxn_t* txn,
    const nrobj_t* x_newrelic_txn);

extern char* nr_header_outbound_request_synthetics_encoded(const nrtxn_t* txn);

extern void nr_header_outbound_request_decoded(nrtxn_t* txn,
                                               char** decoded_id_ptr,
                                               char** decoded_transaction_ptr);

#endif /* NR_HEADER_PRIVATE_HDR */
