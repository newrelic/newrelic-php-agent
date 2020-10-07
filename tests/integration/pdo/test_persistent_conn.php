<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a slow sql trace when a query executed via PDO::query()
using a persistent connection exceeds the explain threshold. This also exercises
the agent's handling of a non-empty options array.
*/

/*SKIPIF
<?php require('skipif_mysql.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "obfuscated" 
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "<unknown>",
      "?? SQL id",
      "select * from tables limit ?;",
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
              "tables",
              "ALL",
              null,
              null,
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

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/pdo.inc');

function test_slow_sql()
{
    global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

    $options = array(PDO::ATTR_PERSISTENT => true);
    $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, $options);
    $result = $conn->query('select * from tables limit 1;');
}

test_slow_sql();
