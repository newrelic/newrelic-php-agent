<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that linking metadata is returned. Trace and span id should be returned
when DT is enabled.
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT
ok - entity name
ok - entity type
ok - host name
ok - trace id
ok - span id
ok - entity guid
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
    [{"name":"Supportability/api/get_linking_metadata"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$metadata = newrelic_get_linking_metadata();

tap_equal(basename(__FILE__), basename($metadata['entity.name']), 'entity name');
tap_equal('SERVICE', $metadata['entity.type'], 'entity type');
tap_equal(gethostname(), $metadata['hostname'], 'host name');

$payload = json_decode(newrelic_create_distributed_trace_payload()->text());
tap_equal($payload->{"d"}->{"tr"}, $metadata['trace.id'], 'trace id');
tap_equal($payload->{"d"}->{"id"}, $metadata['span.id'], 'span id');

tap_assert(!isset($metadata['entity.guid']), 'entity guid');
