<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that newrelic_is_sampled() returns an error when invalid arguments are given
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
  [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
  [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
  [{"name":"Errors/OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
  [{"name":"Errors/all"},                                           [1, "??", "??", "??", "??", "??"]],
  [{"name":"Errors/allOther"},                                      [1, "??", "??", "??", "??", "??"]],
  [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},   [1, "??", "??", "??", "??", "??"]],
  [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
  [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
  [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
  [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
  [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
  [{"name":"Supportability/api/is_sampled"},                        [2, "??", "??", "??", "??", "??"]],
  [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
  [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



/*EXPECT_REGEX
^\s*(PHP )?Fatal error:.*Uncaught ArgumentCountError:.*newrelic_is_sampled\(\) expects exactly 0 arguments, 1 given.*
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
tap_not_equal(newrelic_is_sampled(), newrelic_is_sampled("foo!"),
							"newrelic_is_sampled() unaffected by inclusion of parameter in call");
