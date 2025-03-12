<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that the Supportability metrics
    - "Supportability/DistributedTrace/CreatePayload/Success"
    - "Supportability/TraceContext/Create/Success"
    - "Supportability/api/insert_distributed_trace_headers"
are created when DT headers are inserted into an empty array via the PHP API
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.distributed_tracing_exclude_newrelic_header = false
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/insert_distributed_trace_headers"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT
ok - insert function succeeded
ok - newrelic header is present
ok - tracestate header is present
ok - traceparent header is present
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$headers = array();

tap_assert(newrelic_insert_distributed_trace_headers($headers), 'insert function succeeded');
tap_assert(array_key_exists('newrelic', $headers), 'newrelic header is present');
tap_assert(array_key_exists('tracestate', $headers), 'tracestate header is present');
tap_assert(array_key_exists('traceparent', $headers), 'traceparent header is present');
