<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that linking metadata is returned. No trace and span id should be returned
when DT is disabled.
*/

/*INI
newrelic.distributed_tracing_enabled = false
*/

/*EXPECT
ok - entity name
ok - entity type
ok - host name
ok - entity guid
ok - trace id
ok - span id
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_linking_metadata"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$metadata = newrelic_get_linking_metadata();

tap_equal(basename(__FILE__), basename($metadata['entity.name']), 'entity name');
tap_equal('SERVICE', $metadata['entity.type'], 'entity type');
tap_equal(gethostname(), $metadata['hostname'], 'host name');

tap_assert(isset($metadata['entity.guid']) && $metadata['entity.guid'] !== '', 'entity guid');
tap_assert(!isset($metadata['trace.id']), 'trace id');
tap_assert(!isset($metadata['span.id']), 'span id');
