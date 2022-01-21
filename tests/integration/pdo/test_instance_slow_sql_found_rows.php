<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent shouldn't obliterate MySQL's FOUND_ROWS() value when generating an
explain plan.
*/

/*SKIPIF
<?php require('skipif_mysql.inc');
if (version_compare(PHP_VERSION, "8.1", ">=")) {
  die("skip: PHP >= 8.1.0 not supported\n");
}
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "obfuscated" 
*/

/*EXPECT
ok - found rows
ok - 2 slowsqls
ok - slowsql host matches
ok - slowsql port matches
ok - slowsql database matches
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
      " SELECT FOUND_ROWS() AS r;",
      "Datastore/operation/MySQL/select",
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
    ],
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select ?;",
      "Datastore/operation/MySQL/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in PDO::query called at __FILE__ (??)",
          " in test_slow_sql called at __FILE__ (??)"
        ],
        "explain_plan": [
          [
            "id",
            "select_type",
            "table",
            "type",
            "possible_keys",
            "key",
            "key_len",
            "ref",
            "rows",
            "Extra"
          ],
          [
            [
              "1",
              "SIMPLE",
              null,
              null,
              null,
              null,
              null,
              null,
              "??",
              "??"
            ]
          ]
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
  $conn->query('select 1;');

  // FOUND_ROWS() on older MySQL and MariaDB versions will return 0 here after
  // an explain plan is generated, instead of 1.
  $result = $conn->query('/* comment to prevent explain plan generation */ SELECT FOUND_ROWS() AS r;');
  tap_equal(array('r' => '1'), $result->fetch(PDO::FETCH_ASSOC), 'found rows');
}

test_slow_sql();

$txn = new Transaction;
$slowsqls = $txn->getSlowSQLs();
tap_equal(2, count($slowsqls), '2 slowsqls');
pdo_mysql_assert_datastore_instance_is_valid($slowsqls[0]->getDatastoreInstance());
pdo_mysql_assert_datastore_instance_is_valid($slowsqls[1]->getDatastoreInstance());
pdo_mysql_assert_datastore_instance_metric_exists($txn);
