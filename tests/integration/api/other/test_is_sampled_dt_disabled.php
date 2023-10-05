<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test newrelic_is_sampled() returns false when DT is disabled.
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
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/is_sampled"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT
ok - newrelic_is_sampled() returned false with DT disabled
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
$sampled = newrelic_is_sampled();
tap_equal(false, $sampled, "newrelic_is_sampled() returned false with DT disabled");
