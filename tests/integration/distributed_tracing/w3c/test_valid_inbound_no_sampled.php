<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that a trace context header without sampled of priority is still accepted
correctly.
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
    [{"name":"DurationByCaller/Mobile/332029/2827902/HTTP/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Mobile/332029/2827902/HTTP/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Accept/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_headers"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/Mobile/332029/2827902/HTTP/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/Mobile/332029/2827902/HTTP/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]]
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
        "sampled": "??",
        "priority": "??",
        "traceId": "eb970877cfd349b4dcf5eb9957283bca",
        "parent.app": "2827902",
        "parent.account": "332029",
        "parent.type": "Mobile",
        "parent.transportType": "HTTP",
        "parent.transportDuration": "??",
        "parentSpanId": "5f474d64b9cc9b2a",
        "parentId": "7d3efb1b173fecfa",
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
    "reservoir_size": 1000,
    "events_seen": 1
  },
  [
    [
      {
        "type": "Span",
        "traceId": "eb970877cfd349b4dcf5eb9957283bca",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId" : "5f474d64b9cc9b2a",
        "trustedParentId": "5f474d64b9cc9b2a",
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "parent.type": "Mobile",
        "parent.app": "2827902",
        "parent.account": "332029",
        "parent.transportType": "HTTP",
        "parent.transportDuration": "??"
      }
    ]
  ]
]
*/

$payload = array(
  'trAcepaRent' => "00-eb970877cfd349b4dcf5eb9957283bca-5f474d64b9cc9b2a-00",
  'traCeStAte' => "310705@nr=0-2-332029-2827902-5f474d64b9cc9b2a-7d3efb1b173fecfa---1518469636035"
);

newrelic_accept_distributed_trace_headers($payload, "HTTP");
