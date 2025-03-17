<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that a trace context header parses other vendors correctly.
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability_trusted"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/App/33/5043/HTTPS/all"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/App/33/5043/HTTPS/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Accept/Success"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_headers"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/33/5043/HTTPS/all"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/33/5043/HTTPS/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "guid": "??",
        "sampled": true,
        "priority": 1.234560,
        "traceId": "87b1c9a429205b25e5b687d890d4821f",
        "parent.app": "5043",
        "parent.account": "33",
        "parent.type": "App",
        "parent.transportType": "HTTPS",
        "parent.transportDuration": "??",
        "parentSpanId": "7d3efb1b173fecfa",
        "parentId": "5569065a5b1313bd",
        "error": false
      },
      {},
      {}
    ]
  ]
]
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
        "type": "Span",
        "traceId": "87b1c9a429205b25e5b687d890d4821f",
        "transactionId": "??",
        "sampled": true,
        "priority": 1.234560,
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "7d3efb1b173fecfa",
        "tracingVendors": "dd",
        "trustedParentId": "27ddd2d8890283b4",
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "parent.type": "App",
        "parent.app": "5043",
        "parent.account": "33",
        "parent.transportType": "HTTPS",
        "parent.transportDuration": "??"
      }
    ]
  ]
]
*/

$payload = array(
  'trAcepaRent' => "00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00",
  'traCeStAte' => "dd=YzRiMTIxODk1NmVmZTE4ZQ,{$_ENV['ACCOUNT_supportability_trusted']}@nr=0-0-33-5043-27ddd2d8890283b4-5569065a5b1313bd-1-1.23456-1518469636025"
);

newrelic_accept_distributed_trace_headers($payload, "HTTPS");
