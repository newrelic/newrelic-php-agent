<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli::multi_query.
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
array(1) {
  [0]=>
  string(10) "STATISTICS"
}
array(1) {
  [0]=>
  string(7) "COLUMNS"
}
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_multi_query($link) {
  $query = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'STATISTICS'; ";
  $query .= "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'COLUMNS';";

  if (FALSE === $link->multi_query($query)) {
    echo $link->error . "\n";
    exit(1);
  }

  do {
    if ($result = $link->store_result()) {
      while ($row = $result->fetch_row()) {
        var_dump($row);
      }
      $result->free();
    }
  } while ($link->more_results() && $link->next_result());
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if ($link->connect_errno) {
  echo $link->connect_error . "\n";
  exit(1);
}

test_multi_query($link);
$link->close();
