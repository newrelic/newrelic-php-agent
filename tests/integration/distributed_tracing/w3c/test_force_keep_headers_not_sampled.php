<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that remote_parent_not_sampled = 'always_on' works. Upstream New Relic
tracestate is set to be the opposite of the desired result.
Spans should be sampled with a priority of 2.0, and downstream headers should reflect as such.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing.sampler.remote_parent_not_sampled = 'always_on'
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
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": 2.00000,
        "sampled": true,
        "nr.entryPoint": true,
        "tracingVendors": "123@nr",
        "parentId": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "parent.transportType": "Unknown"
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

$payload = array(
  'traceparent' => "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-00",
  'tracestate' => "123@nr=0-0-1349956-41346604-27ddd2d8890283b4-b28be285632bbc0a-0-1.1273-1569367663277"
);

newrelic_accept_distributed_trace_headers($payload);

$outbound_headers = array('Accept-Language' => 'en-US,en;q=0.5');
tap_assert(newrelic_insert_distributed_trace_headers($outbound_headers), 'insert function succeeded');
$traceparent = explode('-', $outbound_headers['traceparent']);
$tracestate = explode('-', explode('=', $outbound_headers['tracestate'])[1]);

tap_equal($traceparent[3], '01', 'traceparent sampled flag ok');
tap_equal($tracestate[6], '1', 'tracestate sampled flag ok');
tap_equal($tracestate[7], '2.000000', 'tracestate priority ok');
