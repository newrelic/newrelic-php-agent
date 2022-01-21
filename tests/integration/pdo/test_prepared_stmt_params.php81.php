<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a slow sql trace when a prepared
statment with a parameter is executed via PDOStatement::execute()
exceeds the explain threshold.
*/

/*SKIPIF
<?php require('skipif_mysql.inc');
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1.0 not supported\n");
}
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "obfuscated"
*/

/*EXPECT
ok - execute prepared statement with a param
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from tables where table_name = ? limit ?;",
      "Datastore/statement/MySQL/tables/select",
      1,
      "?? total time",
      "?? min time",
      "?? max time",
      {
        "backtrace": [
          " in PDOStatement::execute called at __FILE__ (??)",
          " in test_prepared_statement called at __FILE__ (??)"
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
              1,
              "SIMPLE",
              "tables",
              "ALL",
              null,
              "TABLE_NAME",
              null,
              null,
              "??",
              "??"
            ]
          ]
        ]
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/pdo.inc');

function test_prepared_statement() {
  global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

  $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
  $stmt = $conn->prepare('select * from tables where table_name = ? limit 1;');
  $stmt->bindValue(1, "missing");
  tap_assert($stmt->execute(), 'execute prepared statement with a param');
}

test_prepared_statement();
