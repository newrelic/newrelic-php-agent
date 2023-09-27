<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli_multi_query.
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
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_multi_query($link) {
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'; ";
  $query .= "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='COLUMNS';";

  if (FALSE === mysqli_multi_query($link, $query)) {
    echo $mysqli_error($link);
    return;
  }

  do {
    if ($result = mysqli_store_result($link)) {
      while ($row = mysqli_fetch_row($result)) {
        var_dump($row);
      }
      mysqli_free_result($result);
    }
  } while (mysqli_more_results($link) && mysqli_next_result($link));
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_multi_query($link);
mysqli_close($link);
