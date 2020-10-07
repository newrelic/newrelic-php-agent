<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metric 
"Supportability/DistributedTrace/AcceptPayload/Ignored/MajorVersion"
by sending in a payload with a major version that is larger: 
"v":[1,1] instead of "v":[0,1]
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_payload"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Ignored/MajorVersion"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

$payload = '{"v":[1,1],"d":{"ty":"App","ac":"000000","ap":"1111111","id":"3925aa3552e648dd","tr":"3925aa3552e648dd","pr":1.82236,"sa":true,"ti":1538512769934,"tk":"310705"}}';

newrelic_accept_distributed_trace_payload($payload);
