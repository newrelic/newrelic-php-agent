/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This file contains header manipulation for cross-process tracking.
 */
#ifndef NR_HEADER_HDR
#define NR_HEADER_HDR

#include "nr_app.h"
#include "nr_txn.h"

#define X_NEWRELIC_ID "X-NewRelic-ID"
#define X_NEWRELIC_TRANSACTION "X-NewRelic-Transaction"
#define X_NEWRELIC_APP_DATA "X-NewRelic-App-Data"
#define X_NEWRELIC_APP_DATA_LOWERCASE "x-newrelic-app-data"
#define X_NEWRELIC_SYNTHETICS "X-NewRelic-Synthetics"
#define NEWRELIC "newrelic"
#define W3C_TRACESTATE "tracestate"
#define W3C_TRACEPARENT "traceparent"

/*
 * Request Headers:
 *
 *   X_NEWRELIC_ID
 *
 *   This header contains the encoded cross process id of the application
 *   making the request.
 *
 *   X_NEWRELIC_TRANSACTION
 *
 *   This header contains an encoded json array consisting of:
 *
 *   [client_app_guid_string, record_tt_bool, trip_id, path_hash]
 *
 *   This header is optional, and will only be used by applications which
 *   support cross application tracing.
 *
 *   NEWRELIC
 *
 *   This header contains a base64 encoded json distributed tracing payload.
 *
 *   This header is optional, and will be used by applications which support
 *   distributed tracing.
 *
 * Response Header:
 *
 *   X_NEWRELIC_APP_DATA
 *
 *   This header contains an encoded json array consisting of either:
 *   [
 *     cross_process_id_string,
 *     txn_name_string,
 *     queue_time_float,
 *     response_time_float,
 *     content_length_int
 *   ]
 *   [
 *     cross_process_id_string,
 *     txn_name_string,
 *     queue_time_float,
 *     response_time_float,
 *     content_length_int,
 *     transaction_guid_string,
 *     record_tt_bool
 *   ]
 *   The second version will be used by applications that support
 *   cross application tracing. Note that this agent always returns the second
 *   version regardless of the whether or not the client (requesting) app
 *   supports cross app tracing.  This is because it is expected that all
 *   agents should allow for json arrays that are larger than what they
 *   currently support.  The record_tt_bool may be missing in some agents's
 *   implementations.
 */

/*
 * These are message queue variants of the above names.
 * Lowercase variants are also provided to make matching inbound headers
 * easier: although New Relic specifies exact casing, we should generally prefer
 * to match in a case insensitive manner wherever possible, just in case.
 */
#define X_NEWRELIC_ID_MQ "NewRelicID"
#define X_NEWRELIC_ID_MQ_LOWERCASE "newrelicid"
#define X_NEWRELIC_SYNTHETICS_MQ "NewRelicSynthetics"
#define X_NEWRELIC_SYNTHETICS_MQ_LOWERCASE "newrelicsynthetics"
#define X_NEWRELIC_TRANSACTION_MQ "NewRelicTransaction"
#define X_NEWRELIC_TRANSACTION_MQ_LOWERCASE "newrelictransaction"
#define X_NEWRELIC_DT_PAYLOAD_MQ "newrelic"
#define X_NEWRELIC_DT_PAYLOAD_MQ_LOWERCASE "newrelic"
#define X_NEWRELIC_W3C_TRACEPARENT_MQ "traceparent"
#define X_NEWRELIC_W3C_TRACEPARENT_MQ_LOWERCASE "traceparent"
#define X_NEWRELIC_W3C_TRACESTATE_MQ "tracestate"
#define X_NEWRELIC_W3C_TRACESTATE_MQ_LOWERCASE "tracestate"

typedef enum _nr_response_hdr_field_index_t {
  NR_RESPONSE_HDR_FIELD_INDEX_CROSS_PROCESS_ID = 1,
  NR_RESPONSE_HDR_FIELD_INDEX_TXN_NAME = 2,
  NR_RESPONSE_HDR_FIELD_INDEX_QUEUE_TIME = 3,
  NR_RESPONSE_HDR_FIELD_INDEX_RESPONSE_TIME = 4,
  NR_RESPONSE_HDR_FIELD_INDEX_CONTENT_LENGTH = 5,
  NR_RESPONSE_HDR_FIELD_INDEX_GUID = 6,
  NR_RESPONSE_HDR_FIELD_INDEX_RECORD_TT = 7,

  NR_RESPONSE_HDR_MIN_FIELDS = 5, /* not part of the normal enumeration */
} nr_response_hdr_field_index_t;

/*
 * The largest value that the collector could send is 2147483647#2147483647
 */
#define NR_CROSS_PROCESS_ID_LENGTH_MAX 64

/*
 * Purpose : Create a header map of distributed trace headers.
 *
 * Params : 1. New Relic style distributed trace header.
 *          2. W3C traceparent header.
 *          3. W3C tracestate header.
 *
 * Returns : A hashmap of the New Relic headers. Null on error.
 *
 * Notes : Caller must free header map.
 */
nr_hashmap_t* nr_header_create_distributed_trace_map(const char* nr_header,
                                                     const char* traceparent,
                                                     const char* tracestate);
/*
 * Purpose : Record information from the inbound headers and create the
 *           response header.
 *
 * Params  : 1. The transaction pointer.
 *           2. Content length if it is set in the response headers.
 *              This should be -1 if the content length header is absent.
 *
 * Returns : The X_NEWRELIC_APP_DATA header to be returned to the client.  The
 *           caller must free this string after use.
 *
 * Notes   : For this function to work correctly, nr_header_set_cat_txn must
 *           first be called.
 */
extern char* nr_header_inbound_response(nrtxn_t* txn, int content_length);
/*
 * Purpose : Create headers for an outbound external request.
 *
 * Params  : 1. The current transaction.
 *           2. The current segment.
 *
 * Returns : A hashmap containing the headers.
 *
 * Notes   : The caller must free the hashmap after use.
 */
extern nr_hashmap_t* nr_header_outbound_request_create(nrtxn_t* txn,
                                                       nr_segment_t* segment);

/*
 * Purpose : Process the response header from an outbound external request.
 *
 * Params  : 1. The current transaction.
 *           2. The X_NEWRELIC_APP_DATA response header.
 *           3. Optional location to return the external cross process id.
 *           4. Optional location to return the external transaction name.
 *           5. Optional location to return the external guid.
 *
 * Returns : Nothing. Values parsed from the response header are returned
 *           though the pointer parameters.
 */
extern void nr_header_outbound_response(nrtxn_t* txn,
                                        const char* x_newrelic_app_data,
                                        char** external_id_ptr,
                                        char** external_txnname_ptr,
                                        char** external_guid_ptr);

/*
 * Purpose : Extract the value of a base64 encoded header from a string.
 *
 * Params  : 1. The name of the header.
 *           2. The input string.
 *
 * Returns : A newly allocated string containing the value of the header,
 *           and 0 if it was not found.
 *
 * Example : nr_header_extract_encoded_value ("Data", "XXXX Data: a1b2c3 XXXX")
 *           => "a1b2c3"
 */
extern char* nr_header_extract_encoded_value(const char* header_name,
                                             const char* string);

/*
 * Purpose : Format and return a full header string.
 *
 * Params  : 1. The header name.
 *           2. The header value.
 *           3. Whether or not the string should be suffixed with "\r\n".
 *
 * Returns : A newly allocated string containing the full header or 0 on error.
 *
 * Example :  nr_header_format_name_value ("alpha", "beta", 1) => "alpha:
 *            beta\r\n"
 */
extern char* nr_header_format_name_value(const char* name,
                                         const char* value,
                                         int include_return_newline);

/*
 * Purpose : Extract the mime type from an HTTP Content-Type header.
 *
 * Params  : 1. An HTTP Content-Type header to parse. The expected input format
 *              is either a complete header, e.g. Content-Type: text/html;
 *              charset="utf-8", or the field value, with or without the
 *              charset.
 *
 * Returns : A newly allocated mimetype (possibly empty) string or NULL on
 *           failure.
 *
 * Example : nr_header_parse_content_type ("text/html; charset=\"utf-8\"") =>
 *           "text/html"
 */
extern char* nr_header_parse_content_type(const char* header);

/*
 * Purpose : Add the CAT metadata that was received to the transaction.
 *           This should be called at the beginning of the transaction.
 *
 * Params  : 1. The transaction.
 *           2. The encoded X-NewRelic-ID request header.
 *           3. The encoded X-NewRelic-Transaction request header.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_header_set_cat_txn(nrtxn_t* txn,
                                         const char* x_newrelic_id,
                                         const char* x_newrelic_transaction);

/*
 * Purpose : Add the synthetics metadata that was received to the transaction.
 *
 * Params  : 1. The transaction.
 *           2. The encoded X-NewRelic-Synthetics request header.
 *
 * Returns : NR_SUCCESS or NR_FAILURE.
 */
extern nr_status_t nr_header_set_synthetics_txn(nrtxn_t* txn,
                                                const char* header);

#endif /* NR_HEADER_HDR */
