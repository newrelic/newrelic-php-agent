<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for Postgres prepared statements.
*/

/*SKIPIF
<?php
require("skipif.inc");
require("skipif_php81.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
pg_stats
pg_roles
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/all"},                               [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Postgres/allOther"},                          [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/TABLES/select"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/TABLES/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/VIEWS/select"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/Postgres/VIEWS/select",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Postgres/select"},                  [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
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
      "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_NAME = ?",
      "Datastore/statement/Postgres/VIEWS/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_execute called at __FILE__ (??)",
          " in test_prepare_named called at __FILE__ (??)"
        ]
      }
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL ID",
      "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = ?",
      "Datastore/statement/Postgres/TABLES/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in pg_execute called at __FILE__ (??)",
          " in test_prepare_unnamed called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

function test_prepare_unnamed()
{
  $query = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_NAME = 'pg_stats'";

  if (!pg_prepare('', $query)) {
      echo pg_last_error() . "\n";
      return;
  }

  $result = pg_execute('', array());
  if (!$result) {
      echo pg_last_error() . "\n";
      return;
  }

  while ($row = pg_fetch_row($result)) {
      echo implode(' ', $row) . "\n";
  }
}

function test_prepare_named()
{
  $query = "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_NAME = 'pg_roles'";

  if (!pg_prepare('my query', $query)) {
      echo pg_last_error() . "\n";
      return;
  }

  $result = pg_execute('my query', array());
  if (!$result) {
      echo pg_last_error() . "\n";
      return;
  }

  while ($row = pg_fetch_row($result)) {
      echo implode(' ', $row) . "\n";
  }
}

$conn = pg_connect($PG_CONNECTION);
if (!$conn) {
  exit(1);
}

test_prepare_unnamed();
test_prepare_named();
pg_close($conn);
