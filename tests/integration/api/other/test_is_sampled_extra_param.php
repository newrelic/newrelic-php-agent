<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_is_sampled() returns true when DT is enabled and it is the sole txn
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
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
	  [{"name": "Errors/OtherTransaction/php__FILE__"}, 	[1, "??", "??", "??", "??", "??"]],
	  [{"name": "Errors/all"}, 								[1, "??", "??", "??", "??", "??"]],
	  [{"name": "Errors/allOther"},							[1, "??", "??", "??", "??", "??"]],
	  [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},
	  														[1, "??", "??", "??", "??", "??"]],
	  [{"name": "ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
	  														[1, "??", "??", "??", "??", "??"]],
	  [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
	  [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
	  [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
	  [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
	  [{"name":"Supportability/api/is_sampled"},			[2, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_REGEX
.*Warning.*newrelic_is_sampled\(\) expects exactly 0 parameters, 1 given.*
ok - newrelic_is_sampled\(\) unaffected by inclusion of parameter in call, but logs warning
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
tap_equal(newrelic_is_sampled(), newrelic_is_sampled("foo!"),
          "newrelic_is_sampled() unaffected by inclusion of parameter in call, but logs warning");
