<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should create a databaseDuration attribute when database queries
occur.
*/

/*SKIPIF
<?php require(dirname(__FILE__).'/../pdo/skipif_mysql.inc');
*/

/*EXPECT_ANALYTICS_EVENTS
 [
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "databaseDuration": "??",
        "databaseCallCount": 1,
        "error": false,
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??"
      },
      {
      },
      {
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/../pdo/pdo.inc');

function test_slow_sql() {
  global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

  $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
  $result = $conn->query('select * from tables limit 1;');
}

test_slow_sql();
