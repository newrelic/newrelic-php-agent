<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metrics:
"Supportability/DistributedTrace/CreatePayload/Exception"
"Supportability/DistributedTrace/AcceptPayload/Exception" 
by setting distributed_trace_enabled to false.
 */

/*INI
newrelic.distributed_tracing_enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Errors/OtherTransaction/php__FILE__"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Exception"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/create_distributed_trace_payload"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_payload"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Exception"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

$payload = newrelic_create_distributed_trace_payload();
newrelic_accept_distributed_trace_payload($payload);
