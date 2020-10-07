<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Verify whether the agent correctly instruments subclasses of mysqli and related
classes. 
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
*/

/*EXPECT
STATISTICS
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

/*
 * To test that the agent gracefully handles 
 * re-invoking the mysqli_stmt constructor each time a query is executed. 
 */

class MyDB extends mysqli
{
  function __construct()
  {
    global $MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET;

    parent::__construct($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
  }

  function prepare($query)
  {
    return new MyStatement($this, $query);
  }
}

class MyStatement extends mysqli_stmt
{
  protected $_link = NULL;
  protected $_query = NULL;

  public function __construct($link, $query)
  {
    $this->_link = $link;
    $this->_query = $query;
  }

  public function execute()
  {
    /* It's legal (though not recommended) in PHP to reinvoke the constructor. */
    parent::__construct($this->_link, $this->_query);
    parent::execute();
  }
}

function test_stmt_prepare($link)
{
  $query = "SELECT TABLE_NAME FROM TABLES WHERE TABLE_NAME = 'STATISTICS'";
  $stmt = $link->prepare($query);

  if (FALSE === $stmt->execute() ||
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

function main() {
  $link = new MyDB();
  if ($link->connect_errno) {
    echo $link->connect_error . "\n";
    exit(1);
  }

  test_stmt_prepare($link);
  $link->close();
}

main();
