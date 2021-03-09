<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that trace metadata is returned. Trace and span id should be returned
when Distributed Tracing (DT) is enabled.
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT
ok - trace id
ok - span id
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
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},    
							  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/create_distributed_trace_payload"},
                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_trace_metadata"},    [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$metadata = newrelic_get_trace_metadata();
$payload = json_decode(newrelic_create_distributed_trace_payload()->text());

tap_equal($payload->{"d"}->{"tr"}, $metadata['trace_id'], 'trace id');
tap_equal($payload->{"d"}->{"id"}, $metadata['span_id'], 'span id');
