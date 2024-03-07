<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysql_query.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 0
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/non_existent_table/select"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/non_existent_table/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                                      [1, "??", "??", "??", "??", "??"]],
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
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Unknown table 'non_existent_table' in information_schema",
      "MysqlError",
      {
        "stack_trace": [
          " in mysql_query called at __FILE__ (??)",
          " in test_semantic_error called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? transaction ID"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

/*
 * Pass improperly zero parameters to mysql_query to test how
 * our instrumentation handles parameter parsing failures.
 */
function test_no_args()
{
  mysql_query();
}

/*
 * Pass improperly typed parameters to mysql_query to test how
 * our instrumentation handles parameter parsing failures.
 */
function test_type_mismatch($link)
{
  /* Ignore "mysql_query() expects parameter 1 to be string, array given" warning. */
  @mysql_query(array(42), $link);
}

/* Test running a well-formed query that results in an error. */
function test_semantic_error($link)
{
  $query = "SELECT * FROM non_existent_table where password='sshhhh';";
  mysql_query($query, $link);
}

$link = mysql_connect($MYSQL_SERVER, $MYSQL_USER, $MYSQL_PASSWD);
if (FALSE === $link) {
  echo mysql_error() . "\n";
  exit(1);
}

if (!empty($MYSQL_DB)) {
  if (FALSE === mysql_select_db($MYSQL_DB, $link)) {
    echo mysql_error() . "\n";
    exit(1);
  }
}

test_no_args();
test_type_mismatch($link);
test_semantic_error($link);
mysql_close($link);
