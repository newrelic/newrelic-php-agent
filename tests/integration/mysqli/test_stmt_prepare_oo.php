<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Database metrics for mysqli_stmt objects.
Note that, for the object-oriented method, we do not send instance metrics or
attributes.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
TRIGGERS
STATISTICS
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                           [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},                [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select"}, [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/statement/MySQL/tables/select",
      "scope":"OtherTransaction/php__FILE__"},           [2, "??", "??", "??", "??", "??"]],
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

function test_stmt_construct($link)
{
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='TRIGGERS'";
  $stmt = new mysqli_stmt($link, $query);

  if (FALSE === $stmt->execute() ||
      FALSE === $stmt->bind_result($name)) {
    echo $stmt->error . "\n";
    $stmt->close();
    return;
  }

  $stmt->fetch();
  echo $name . "\n";

  $stmt->close();
}

function test_stmt_prepare($link)
{
  $stmt = $link->stmt_init();
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name='STATISTICS'";

  if (FALSE === $stmt->prepare($query) ||
      FALSE === $stmt->execute() ||
      FALSE === $stmt->bind_result($name)) {
    echo $stmt->error . "\n";
    $stmt->close();
    return;
  }

  while ($stmt->fetch()) {
    echo $name . "\n";
  }

  $stmt->close();
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if ($link->connect_errno) {
  echo $link->connect_error . "\n";
  exit(1);
}

test_stmt_construct($link);
test_stmt_prepare($link);
$link->close();
