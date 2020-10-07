/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NR_DISTRIBUTED_TRACE_PRIVATE_HDR
#define NR_DISTRIBUTED_TRACE_PRIVATE_HDR

#include "util_sampling.h"
#include "util_time.h"

static const int NR_DISTRIBUTED_TRACE_VERSION_MAJOR = 0;
static const int NR_DISTRIBUTED_TRACE_VERSION_MINOR = 1;

/*
 * Distributed Tracing Metadata
 *
 * This type's job is to keep track of any and all metadata needed
 * by the distributed tracing functionality (the create payload function, the
 * accept payload function, any intrinsic setting code, etc.), and serve as the
 * "source of truth" for of any bit of distributed trace metadata that's shared
 * between the distinct parts of distributed tracing.
 *
 * These fields will not be accessed directly -- instead use the functions
 * in nr_distributed_trace.h
 *
 * (see also: the distributed_trace field in the nrtxn_t type)
 */
struct _nr_distributed_trace_t {
  char* account_id; /* The APM account identifier */
  char* app_id;     /* The application identifier (i.e. cluster agent ID) */
  char* txn_id;     /* The transaction guid. */
  nr_sampling_priority_t priority; /* Likelihood to be saved */
  bool sampled;                    /* Whether this trip should be sampled */
  char* trace_id;    /* Links all spans within the call chain together */
  char* trusted_key; /* Trusted account key from the connect service */

  /* These fields are set when a distributed trace is accepted */
  struct {
    bool set;         /* Set to true when inbound has been accepted */
    char* type;       /* Contains "App", "Browser", or "Mobile" */
    char* app_id;     /* The application identifier (i.e. cluster agent ID) */
    char* account_id; /* The APM account identifier */
    char* transport_type;  /* How the inbound payload was transported */
    nrtime_t timestamp;    /* payload timestamp */
    char* guid;            /* The inbound span identifier. */
    char* txn_id;          /* The inbound transaction guid. */
    char* tracing_vendors; /* List of other vendors that were parsed from the
                              W3C tracestate header */
    char* raw_tracing_vendors; /* List of raw trace state headers from other
                                  vendors */
    char* trusted_parent_id; /* The spanId from a New Relic W3C tracestate entry
                                with a matching trusted account key */
  } inbound;
};

/*
 * Distributed Tracing Payload
 *
 * A transaction may make multiple outbound requests.  This type's job is
 * to keep track of any request-specific information that will be placed
 * in the payload.  Note that it also points to a
 * struct _nr_distributed_trace_t which contains the distributed trace
 * metadata that does not change during a transaction.
 */
struct _nr_distributed_trace_payload_t {
  const struct _nr_distributed_trace_t*
      metadata; /* A pointer to the transaction's distributed trace metadata */
  char* parent_id;    /* The caller's span ID */
  nrtime_t timestamp; /* When the payload was created */
};

#endif /* NR_DISTRIBUTED_TRACE_PRIVATE_HDR */
