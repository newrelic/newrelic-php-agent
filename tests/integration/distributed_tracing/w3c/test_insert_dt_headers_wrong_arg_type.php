<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that non-array arguments are gracefully rejected
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
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
    [{"name":"Supportability/api/insert_distributed_trace_headers"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_REGEX
Warning: newrelic_insert_distributed_trace_headers\(\) expects parameter 1 to be array,.*
ok - rejected non-array argument
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

$string_arg = "howdy";
tap_assert(!newrelic_insert_distributed_trace_headers($string_arg), 'rejected non-array argument');
