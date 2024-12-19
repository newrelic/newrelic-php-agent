<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a slow sql trace when a query executed via PDO::query()
exceeds the explain threshold includes a non-terminal semi-colon.
*/

/*SKIPIF
<?php require('skipif_mysql.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "obfuscated" 
*/

/*EXPECT
ok - 1 slowsql
ok - slowsql host matches
ok - slowsql port matches
ok - slowsql database matches
ok - datastore instance metric exists
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from information_schema.tables where engine = ?;",
      "Datastore/statement/MySQL/tables/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in PDO::query called at __FILE__ (??)",
          " in test_slow_sql called at __FILE__ (??)"
        ],
        "host": "??",
        "port_path_or_id": "??",
        "database_name": "??"
      }
    ]
  ]
]
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/pdo.inc');

function test_slow_sql() {
  global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

  $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
  $result = $conn->query('select * from information_schema.tables where engine = \';\';');
}

test_slow_sql();

$txn = new Transaction;
$slowsqls = $txn->getSlowSQLs();
tap_equal(1, count($slowsqls), '1 slowsql');
pdo_mysql_assert_datastore_instance_is_valid($slowsqls[0]->getDatastoreInstance());
pdo_mysql_assert_datastore_instance_metric_exists($txn);
