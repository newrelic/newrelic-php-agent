<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that remote_parent_not_sampled = 'always_on' works.
Spans should be sampled with a priority of 2.0, and downstream headers should reflect as such.
*/

/*INI
newrelic.transaction_events.attributes.include=request.uri
newrelic.distributed_tracing.sampler.remote_parent_not_sampled = 'always_on'
*/

/*HEADERS
traceparent=00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00
*/

/*ENVIRONMENT
REQUEST_METHOD=POST
CONTENT_LENGTH=348
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 1
  },
  [
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "WebTransaction\/Uri__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": 2.00000,
        "sampled": true,
        "nr.entryPoint": true,
        "parentId": "??",
        "transaction.name": "WebTransaction\/Uri__FILE__"
      },
      {},
      {
        "parent.transportType": "HTTP",
        "response.headers.contentType": "text\/html",
        "http.statusCode": 200,
        "response.statusCode": 200,
        "httpResponseCode": "200",
        "request.uri": "__FILE__",
        "request.method": "POST",
        "request.headers.host": "??",
        "request.headers.contentLength": "??"

      }
    ]
  ]
]
*/

/*EXPECT
ok - insert function succeeded
ok - traceparent sampled flag ok
ok - tracestate sampled flag ok
ok - tracestate priority ok
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$outbound_headers = array('Accept-Language' => 'en-US,en;q=0.5');
tap_assert(newrelic_insert_distributed_trace_headers($outbound_headers), 'insert function succeeded');
$traceparent = explode('-', $outbound_headers['traceparent']);
$tracestate = explode('-', explode('=', $outbound_headers['tracestate'])[1]);

tap_equal($traceparent[3], '01', 'traceparent sampled flag ok');
tap_equal($tracestate[6], '1', 'tracestate sampled flag ok');
tap_equal($tracestate[7], '2.000000', 'tracestate priority ok');
