/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <stdio.h>

#include "nr_header.h"
#include "nr_segment_external.h"
#include "nr_segment_private.h"
#include "util_strings.h"
#include "util_url.h"

/*
 * Purpose : Set all the typed external attributes on the segment.
 */
static void nr_segment_external_set_attrs(
    nr_segment_t* segment,
    const nr_segment_external_params_t* params,
    char* external_guid) {
  nr_segment_external_t attrs = {0};

  if (params->uri) {
    attrs.uri = nr_url_clean(params->uri, nr_strlen(params->uri));
  }
  attrs.library = params->library;
  attrs.procedure = params->procedure;
  attrs.transaction_guid = external_guid;
  attrs.status = params->status;

  nr_segment_set_external(segment, &attrs);

  nr_free(attrs.uri);
}

/*
 * Purpose : Create metrics for a completed external call and set the segment
 *           name.
 *
 * Metrics created during this call
 * ----------------------------------------------------------------------------
 * External/all                                                Unscoped Always
 * External/{host}/all                                         Scoped   non-CAT
 * External/{host}/all                                         Unscoped CAT
 * ExternalApp/{host}/{external_id}/all                        Unscoped CAT
 * ExternalTransaction/{host}/{external_id}/{external_txnname} Scoped   CAT
 *
 * Metrics created based on External/all (in nr_txn_create_rollup_metrics)
 * ----------------------------------------------------------------------------
 * External/allWeb                                             Unscoped Web
 * External/allOther                                           Unscoped non-Web
 *
 * Segment name
 * ----------------------------------------------------------------------------
 * External/{host}/all                                                  non-CAT
 * ExternalTransaction/{host}/{external_id}/{external_txnname}          CAT
 *
 * These metrics are dictated by the agent-spec in this file:
 * Cross-Application-Tracing-PORTED.md
 */
static void nr_segment_external_create_metrics(nr_segment_t* segment,
                                               const char* uri,
                                               const char* external_id,
                                               const char* external_txnname) {
  char buf[1024] = {0};
  size_t buflen = sizeof(buf);
  const char* domain;
  int domainlen = 0;

#define ADD_METRIC(M_scoped, ...)     \
  snprintf(buf, buflen, __VA_ARGS__); \
  nr_segment_add_metric(segment, buf, M_scoped);

  domain = nr_url_extract_domain(uri, nr_strlen(uri), &domainlen);
  if ((NULL == domain) || (domainlen <= 0)
      || (size_t)domainlen >= (buflen - 256)) {
    domain = "<unknown>";
    domainlen = nr_strlen(domain);
  }

  /* Rollup metric.
   *
   * This has to be created on the transaction in order to create
   * External/allWeb and External/allOther and to calculate
   * externalDuration later on.
   */
  nrm_force_add(segment->txn->unscoped_metrics, "External/all",
                nr_time_duration(segment->start_time, segment->stop_time));

  if (external_id && external_txnname) {
    /* Metrics in case of CAT */
    ADD_METRIC(false, "External/%.*s/all", domainlen, domain);
    ADD_METRIC(false, "ExternalApp/%.*s/%s/all", domainlen, domain,
               external_id);
    ADD_METRIC(true, "ExternalTransaction/%.*s/%s/%s", domainlen, domain,
               external_id, external_txnname);
  } else {
    /* Metrics in case of not-CAT */
    ADD_METRIC(true, "External/%.*s/all", domainlen, domain);
  }

  /* buf now contains the name of the scoped metric. This is also used as
   * the segment name. */
  segment->name = nr_string_add(segment->txn->trace_strings, buf);
}

bool nr_segment_external_end(nr_segment_t** segment_ptr,
                             const nr_segment_external_params_t* params) {
  char* external_id = NULL;
  char* external_txnname = NULL;
  char* external_guid = NULL;
  bool rv = false;
  nrtime_t start;
  nr_segment_t* segment;

  if (NULL == segment_ptr) {
    return false;
  }

  segment = *segment_ptr;

  if (NULL == segment || NULL == params || NULL == segment->txn) {
    return false;
  }

  if (params->encoded_response_header) {
    nr_header_outbound_response(segment->txn, params->encoded_response_header,
                                &external_id, &external_txnname,
                                &external_guid);
  }

  nr_segment_external_set_attrs(segment, params, external_guid);

  start = segment->start_time;
  nr_segment_set_timing(segment, start,
                        nr_time_duration(start, nr_txn_now_rel(segment->txn)));

  nr_segment_external_create_metrics(segment, params->uri, external_id,
                                     external_txnname);

  rv = nr_segment_end(&segment);

  nr_free(external_id);
  nr_free(external_txnname);
  nr_free(external_guid);

  return rv;
}
