<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysql_query.
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 0
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"ErrorsByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/non_existent_table/select"},
                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/non_existent_table/select",
      "scope":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/OtherTransaction/php__FILE__"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/all"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Errors/allOther"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [1, "??", "??", "??", "??", "??"]]
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
          " in mysql_db_query called at __FILE__ (??)",
          " in test_semantic_error called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

/*
 * Pass an invalid number of zero parameters to mysql_db_query to test how
 * our instrumentation handles parameter parsing failures.
 */
function test_no_args()
{
  @mysql_db_query();
}

/*
 * Pass improperly typed parameters to mysql_db_query to test how
 * our instrumentation handles parameter parsing failures.
 */
function test_type_mismatch($link, $db)
{
  /* Ignore "mysql_db_query() expects parameter 1 to be string, array given" warning. */
  @mysql_db_query(array(42), "SELECT CURRENT_USER();", $link);

  /* Ignore "mysql_db_query() expects parameter 2 to be string, array given" warning. */
  @mysql_db_query($db, array(42), $link);
}

/* Test running a well-formed query that results in an error. */
function test_semantic_error($link, $db)
{
  $query = "SELECT * FROM non_existent_table where password='sshhhh';";
  mysql_db_query($db, $query, $link);
}

$link = mysql_connect($MYSQL_SERVER, $MYSQL_USER, $MYSQL_PASSWD);
if (FALSE === $link) {
  echo mysql_error() . "\n";
  exit(1);
}

test_no_args();
test_type_mismatch($link, $MYSQL_DB);
test_semantic_error($link, $MYSQL_DB);
mysql_close($link);
