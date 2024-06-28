/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DISTRIBUTED_TRACE_HDR
#define NR_DISTRIBUTED_TRACE_HDR

#include <stdbool.h>

#include "util_sampling.h"
#include "util_time.h"
#include "util_object.h"

#define NR_TRACE_ID_SIZE 32

static const char NR_DISTRIBUTED_TRACE_ACCEPT_SUCCESS[]
    = "Supportability/DistributedTrace/AcceptPayload/Success";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_EXCEPTION[]
    = "Supportability/DistributedTrace/AcceptPayload/Exception";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_PARSE_EXCEPTION[]
    = "Supportability/DistributedTrace/AcceptPayload/ParseException";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_CREATE_BEFORE_ACCEPT[]
    = "Supportability/DistributedTrace/AcceptPayload/Ignored/"
      "CreateBeforeAccept";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_MULTIPLE[]
    = "Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_MAJOR_VERSION[]
    = "Supportability/DistributedTrace/AcceptPayload/Ignored/MajorVersion";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_NULL[]
    = "Supportability/DistributedTrace/AcceptPayload/Ignored/Null";
static const char NR_DISTRIBUTED_TRACE_ACCEPT_UNTRUSTED_ACCOUNT[]
    = "Supportability/DistributedTrace/AcceptPayload/Ignored/UntrustedAccount";
static const char NR_DISTRIBUTED_TRACE_CREATE_SUCCESS[]
    = "Supportability/DistributedTrace/CreatePayload/Success";
static const char NR_DISTRIBUTED_TRACE_CREATE_EXCEPTION[]
    = "Supportability/DistributedTrace/CreatePayload/Exception";
static const char NR_DISTRIBUTED_TRACE_W3C_CREATE_SUCCESS[]
    = "Supportability/TraceContext/Create/Success";
static const char NR_DISTRIBUTED_TRACE_W3C_CREATE_EXCEPTION[]
    = "Supportability/TraceContext/Create/Exception";
static const char NR_DISTRIBUTED_TRACE_W3C_ACCEPT_SUCCESS[]
    = "Supportability/TraceContext/Accept/Success";
static const char NR_DISTRIBUTED_TRACE_W3C_ACCEPT_EXCEPTION[]
    = "Supportability/TraceContext/Accept/Exception";
static const char NR_DISTRIBUTED_TRACE_W3C_TRACEPARENT_PARSE_EXCEPTION[]
    = "Supportability/TraceContext/TraceParent/Parse/Exception";
static const char NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_PARSE_EXCEPTION[]
    = "Supportability/TraceContext/TraceState/Parse/Exception";
static const char NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_NONRENTRY[]
    = "Supportability/TraceContext/TraceState/NoNrEntry";
static const char NR_DISTRIBUTED_TRACE_W3C_TRACESTATE_INVALIDNRENTRY[]
    = "Supportability/TraceContext/TraceState/InvalidNrEntry";
static const char NR_DISTRIBUTED_TRACE_W3C_TRACECONTEXT_ACCEPT_EXCEPTION[]
    = "Supportability/TraceContext/Accept/Exception";
/*
 * Purpose : The public version of the distributed trace structs/types
 */
typedef struct _nr_distributed_trace_t nr_distributed_trace_t;
typedef struct _nr_distributed_trace_payload_t nr_distributed_trace_payload_t;

/*
 * Purpose : Creates/allocates a new distributed tracing metadata struct
 *           instance.  It's the responsibility of the caller to
 *           free/destroy the struct with the nr_distributed_trace_destroy
 *           function.
 *
 * Params  : None.
 *
 * Returns : An allocated nr_distributed_trace_t that the caller owns and must
             destroy with nr_distributed_trace_destroy().
 */
nr_distributed_trace_t* nr_distributed_trace_create(void);

/*
 * Purpose : Accepts an inbound distributed trace with an nrobj payload.
 *           The payload will copied to the inbound struct within
 *           the distributed trace.
 *
 * Params  : 1. A properly allocated distributed trace
 *           2. A nrobj containing the converted JSON payload
 *           3. The transport type of the payload, which has to be one of
 *              "Unknown", "HTTP", "HTTPS", "Kafka", "JMS", "IronMQ", "AMQP",
 *              "Queue" or "Other".
 *           4. An error string to be populated if an error occurs
 *
 * Returns : True on success, otherwise return false with a populated error
 *           string detailing the supportability metric name to report by the
 *           caller.
 */
bool nr_distributed_trace_accept_inbound_payload(nr_distributed_trace_t* dt,
                                                 const nrobj_t* obj_payload,
                                                 const char* transport_type,
                                                 const char** error);

/*
 * Purpose : Accepts a JSON payload, validates the payload and format, and
 *           returns an nrobj version of that payload.
 *
 * Params  : 1. A JSON payload
 *           2. An error string to be populated if an error occurs
 *
 * Returns : an nrobj on success, otherwise NULL with a populated error string
 *           detailing the supportability metric name to report by the caller.
 */
nrobj_t* nr_distributed_trace_convert_payload_to_object(const char* payload,
                                                        const char** error);

/*
 * Purpose : Accepts W3C TraceContext headers and returns an nrobj version of
 *           the information.
 *
 *           To avoid another intermediate data structure, the parsed
 *           information from headers is returned in an nrobj with the following
 *           layout:
 *
 *           {
 *               "traceparent" : {
 *                   "version" : <string>,
 *                   "trace_id" : <string>,
 *                   "parent_id" : <string>,
 *                   "trace_flags" : <int>
 *               },
 *               "tracingVendors" : <string>,
 *               "rawTracingVendors": [ <string> ],
 *               "tracestate" : {
 *                   "version" : <int>,
 *                   "parent_type" : <int>,
 *                   "parent_account_id" : <string>,
 *                   "parent_application_id" : <string>,
 *                   "span_id" : <string>,
 *                   "transaction_id" : <string>,
 *                   "sampled" : <int>,
 *                   "priority" : <float>,
 *                   "timestamp" : <int>
 *               }
 *           }
 *
 *           Optional items that were not specified in the trace state header
 *           will be omitted in this output.
 *
 * Params  : 1. A W3C trace parent header value.
 *           2. A W3C trace state header value.
 *           3. The trusted account id.
 *           4. An error string to be populated if an error occurs
 *
 * Returns : An nrobj on success, otherwise NULL with a populated error string
 *           detailing the supportability metric name to report by the caller.
 */
nrobj_t* nr_distributed_trace_convert_w3c_headers_to_object(
    const char* traceparent,
    const char* tracestate,
    const char* trusted_account_id,
    const char** error);

/*
 * Purpose : Destroys/frees structs created via nr_distributed_trace_create
 *
 * Params  : A pointer to the pointer that points at the allocated
 *           nr_distributed_trace_t (created with nr_distributed_trace_create)
 *
 * Returns : nothing
 */
void nr_distributed_trace_destroy(nr_distributed_trace_t** ptr);

/*
 * Purpose : Get the various fields of the distributed trace metadata
 *
 * Params  : 1. The distributed trace.
 *
 * Returns : The stated field.
 */
extern const char* nr_distributed_trace_get_account_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_get_app_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_get_txn_id(
    const nr_distributed_trace_t* dt);
extern nr_sampling_priority_t nr_distributed_trace_get_priority(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_get_trace_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_get_trusted_key(
    const nr_distributed_trace_t* dt);
bool nr_distributed_trace_is_sampled(const nr_distributed_trace_t* dt);
bool nr_distributed_trace_inbound_is_set(const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_account_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_app_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_guid(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_txn_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_type(
    const nr_distributed_trace_t* dt);
extern nrtime_t nr_distributed_trace_inbound_get_timestamp_delta(
    const nr_distributed_trace_t* dt,
    nrtime_t txn_start);
extern bool nr_distributed_trace_inbound_has_timestamp(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_transport_type(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_tracing_vendors(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_raw_tracing_vendors(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_inbound_get_trusted_parent_id(
    const nr_distributed_trace_t* dt);
extern const char* nr_distributed_trace_object_get_account_id(
    const nrobj_t* object);
extern const char* nr_distributed_trace_object_get_trusted_key(
    const nrobj_t* object);

/*
 * Purpose : Set the transaction id.
 *
 * Params  : 1. The distributed trace.
 *           2. The transaction id.
 */
extern void nr_distributed_trace_set_txn_id(nr_distributed_trace_t* dt,
                                            const char* guid);

/*
 * Purpose : Set the Account ID.
 *
 * Params  : 1. The distributed trace.
 *           2. The Account ID.
 */
extern void nr_distributed_trace_set_account_id(nr_distributed_trace_t* dt,
                                                const char* account_id);

/*
 * Purpose : Set the distributed trace App ID.
 *
 * Params  : 1. The distributed trace.
 *           2. The App ID.
 */
extern void nr_distributed_trace_set_app_id(nr_distributed_trace_t* dt,
                                            const char* app_id);

/*
 * Purpose : Set the distributed trace trace id.
 *
 * Params  : 1. The distributed trace.
 *           2. The trace id.
 */
void nr_distributed_trace_set_trace_id(nr_distributed_trace_t* dt,
                                       const char* trace_id);

/*
 * Purpose : Set the distributed trace priority.
 *
 * Params  : 1. The distributed trace.
 *           2. The sampling priority.
 */
extern void nr_distributed_trace_set_priority(nr_distributed_trace_t* dt,
                                              nr_sampling_priority_t priority);

/*
 * Purpose : Set the trusted account key.
 *
 * Params  : 1. The distributed trace.
 *           2. The trusted account key.
 */
extern void nr_distributed_trace_set_trusted_key(nr_distributed_trace_t* dt,
                                                 const char* trusted_key);

/*
 * Purpose : Set the tracing vendors list.
 *
 * Params  : 1. The distributed trace.
 *           2. tracing vendors list parsed from a W3C tracestate header.
 */
extern void nr_distributed_trace_inbound_set_tracing_vendors(
    nr_distributed_trace_t* dt,
    const char* other_vendors);

/*
 * Purpose : Set the trusted parent id.
 *
 * Params  : 1. The distributed trace.
 *           2. trusted parent id.
 */
extern void nr_distributed_trace_inbound_set_trusted_parent_id(
    nr_distributed_trace_t* dt,
    const char* trusted_parent_id);
/*
 * Purpose :  Setter function for the sampled property
 *
 * Params  :  1. The nr_distributed_trace_t* whose property we want to set
 *            2. The bool value to set for the sampled property
 */
void nr_distributed_trace_set_sampled(nr_distributed_trace_t* dt, bool value);

/*
 * Purpose :  Setter function for the transport type
 *
 * Params  :  1. the nr_distributed_trace_t* whose property we want to set
 *            2. the string to set for the transport type
 */
void nr_distributed_trace_inbound_set_transport_type(nr_distributed_trace_t* dt,
                                                     const char* value);

/*
 * Purpose :  Create/allocates a new distributed tracing payload instance,
 *
 * Params  :  1. A pointer to the distributed trace metadata for this payload.
 *            2. The initial value for parent_id.
 *
 * Returns :  An allocated nr_distributed_trace_t that the caller
 *            must destroy with nr_distributed_trace_payload_destroy
 */
nr_distributed_trace_payload_t* nr_distributed_trace_payload_create(
    const nr_distributed_trace_t* metadata,
    const char* parent_id);

/*
 * Purpose : Destroys/frees structs created via
 *           nr_distributed_trace_payload_create
 *
 * Params  :  A pointer to the pointer that points at the allocated
 *            nr_distributed_trace_payload_t (created with
 *            nr_distributed_trace_payload_create)
 *
 * Returns : nothing
 */
void nr_distributed_trace_payload_destroy(nr_distributed_trace_payload_t** ptr);

/*
 * Purpose : Get the various fields of the distributed trace payload
 *
 * Params  : The distributed trace payload.
 *
 * Returns : The stated field.
 */
const char* nr_distributed_trace_payload_get_parent_id(
    const nr_distributed_trace_payload_t* payload);
nrtime_t nr_distributed_trace_payload_get_timestamp(
    const nr_distributed_trace_payload_t* payload);
const nr_distributed_trace_t* nr_distributed_trace_payload_get_metadata(
    const nr_distributed_trace_payload_t* payload);

/*
 * Purpose : Create the text representation of the distributed trace payload.
 *
 * Params  : 1. The distributed trace payload.
 *
 * Returns : A newly allocated, null terminated payload string, which the caller
 *           must destroy with nr_free() when no longer needed, or NULL on
 *           error.
 */
extern char* nr_distributed_trace_payload_as_text(
    const nr_distributed_trace_payload_t* payload);

/*
 * Purpose : Create a W3C tracestate header for distributed tracing.
 *
 * Params  : 1. The distributed trace object from the current transaction.
 *           2. The current segment ID.
 *           3. The current transaction ID.
 *
 * Returns : A newly created W3C tracestate header for distributed tracing. If
 *           the call fails NULL will be returned. It is the callers
 *           responsibility to free the string.
 *
 * Note : The transaction id and span id should be omitted based on
 *        configuration values. It is the callers responsibility to check
 *        those before sending them. This function will omit these values if
 *        NULL is passed in.
 */
char* nr_distributed_trace_create_w3c_tracestate_header(
    const nr_distributed_trace_t* dt,
    const char* span_id,
    const char* txn_id);

/*
 * Purpose : Create a W3C trace parent header.
 *
 * Params : 1. The current trace id.
 *          2. The current span id.
 *          3. If the trace is sampled
 *
 * Returns : A newly allocated W3C trace parent header. Caller is responsible
 *           for destroying the return value. If span id or trace id are NULL
 *           then NULL will be returned.
 */
char* nr_distributed_trace_create_w3c_traceparent_header(const char* trace_id,
                                                         const char* span_id,
                                                         bool sampled);

/*
 * Purpose : Accept a W3C header.
 *
 * Params : 1. The distributed trace object.
 *          2. The trace headers as an object.
 *          3. Transport type.
 *          4. Errors
 *
 * Returns : True on success, false on error.
 */
bool nr_distributed_trace_accept_inbound_w3c_payload(
    nr_distributed_trace_t* dt,
    const nrobj_t* trace_headers,
    const char* transport_type,
    const char** error);

#endif /* NR_DISTRIBUTED_TRACE_HDR */
