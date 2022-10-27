<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for mysqli_query.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
ok - test_mysqli_query2 (query)
ok - test_mysqli_query2 (fetch)
ok - test_mysqli_query3 (query)
ok - test_mysqli_query3 (fetch)
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                 [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                           [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"},            [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

/* Test the two argument form. */
function test_mysqli_query2($link)
{
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";
  $result = mysqli_query($link, $query);
  tap_not_equal(FALSE, $result, "test_mysqli_query2 (query)");
  $expected = array(0 => "STATISTICS", "TABLE_NAME" => "STATISTICS");

  if (FALSE !== $result) {
    while ($row = mysqli_fetch_array($result)) {
      tap_equal($expected, $row, "test_mysqli_query2 (fetch)");
    }
    mysqli_free_result($result);
  }
}

/* Test the three argument form. */
function test_mysqli_query3($link)
{

  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";
  $result = mysqli_query($link, $query, MYSQLI_USE_RESULT);
  
  tap_not_equal(FALSE, $result, "test_mysqli_query3 (query)");

  $expected = array(0 => "STATISTICS", "TABLE_NAME" => "STATISTICS");

  if (FALSE !== $result) {
    while ($row = mysqli_fetch_array($result)) {
      tap_equal($expected, $row, "test_mysqli_query3 (fetch)");
    }
    mysqli_free_result($result);
  }
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_mysqli_query2($link);
test_mysqli_query3($link);
mysqli_close($link);
