<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for mysql_query.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
error_reporting = E_ALL &~ E_DEPRECATED
*/

/*EXPECT
array(1) {
  [0]=>
  string(10) "STATISTICS"
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
    [{"name":"Datastore/operation/MySQL/select"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/



/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = ?;",
      "Datastore/statement/MySQL/TABLES/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in mysql_db_query called at __FILE__ (??)",
          " in test_mysql_db_query called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function test_mysql_db_query($link, $db)
{
  $query = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'STATISTICS';";

  $result = mysql_db_query($db, $query, $link);
  if (FALSE === $result) {
    echo mysql_error() . "\n";
    return;
  }

  while ($row = mysql_fetch_array($result, MYSQL_NUM)) {
    var_dump($row);
  }

  mysql_free_result($result);
}

$link = mysql_connect($MYSQL_SERVER, $MYSQL_USER, $MYSQL_PASSWD);
if (FALSE === $link) {
  echo mysql_error() . "\n";
  exit(1);
}

test_mysql_db_query($link, $MYSQL_DB);
mysql_close($link);
