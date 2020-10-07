<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metric "Supportability/DistributedTrace/AcceptPayload/Success"
when the payload is correct.
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
    [{"name":"DurationByCaller/App/000000/1111111/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/App/000000/1111111/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_payload"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/432507/4741547/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/432507/4741547/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

$payload = '{"v":[0,1],"d":{"ac":"000000","ap":"1111111","id":"2222222222222222","tr":"4444444444444444","pr":1.55555,"sa":true,"ti":7777777777777,"tk":"888888"}}';

newrelic_accept_distributed_trace_payload($payload);
