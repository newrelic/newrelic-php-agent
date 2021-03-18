<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that no linking metadata is returned when invalid arguments are given.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled = false
 */

/*EXPECT_REGEX
.*Warning.*newrelic_get_linking_metadata\(\) expects exactly 0 parameters, 1 given.*
ok - empty metadata
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
    [{"name":"Errors/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_linking_metadata"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

$metadata = newrelic_get_linking_metadata("arg");

tap_equal(array(), $metadata, 'empty metadata');
