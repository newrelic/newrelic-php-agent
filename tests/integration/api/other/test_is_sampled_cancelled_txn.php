<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_is_sampled() returns true when Distributed Tracing (DT) is enabled and it is the sole
transaction.
*/
/*INI
newrelic.distributed_tracing_enabled = true
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
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
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
	  [{"name":"Supportability/api/end_transaction"},		[1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT
ok - newrelic_is_sampled() returns false when transaction is manually ended
*/
require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
newrelic_end_transaction();
$sampled = newrelic_is_sampled();
tap_equal($sampled, false,
          "newrelic_is_sampled() returns false when transaction is manually ended");

