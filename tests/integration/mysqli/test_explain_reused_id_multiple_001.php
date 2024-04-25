<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should generate explain plans even when object IDs are reused
and some of the SQL strings are not explainable.  This tests that if
a object ID is reused and a new mysqli::stmt object is created with an
unexplainable SQL string that the "mysqli_entries" entry for this
object ID ("handle") has it "query" parameter cleared.  This prevents
using an older, stale "query" parameter (from a myqsli::stmt object
which was reclaimed by PHP) being using incorrectly with a newer query.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 500
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT_SLOW_SQLS
[
  [
    [
      "OtherTransaction/php__FILE__",
      "\u003cunknown\u003e",
      "??",
      "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=? AND SLEEP(?);;",
      "Datastore/statement/MySQL/tables/select",
      1,
      "??",
      "??",
      "??",
      {
        "backtrace": [
          " in mysqli_stmt::execute called at __FILE__ (??)",
          " in doSQL called at __FILE__ (??)"
        ]
      }
    ]
  ]
]
*/

/*EXPECT
If you see this message, it didn't crash
*/

/*EXPECT_TRACED_ERRORS
null
*/

/* Create a mysqli::stmt object using "prepare" but do not execute.
   The result will be the mysqli:stmt object will be released when
   this function exits and be reused.
 */
function justPrepare($dbConn)
{
    $stmt2 = $dbConn->prepare("SELECT ?");
}

/* Run an SQL query.
   Params:
      $dbConn         mysql connection object
      $delay          delay for query in seconds
      $explainable    true if query is explainable, otherwise an
                      unexplainable sql query is run
*/
function doSQL($dbConn, $delay, $explainable) {

    /* if sql statement has 2 ';' then agent considers it as multi-statement
       and treats it as unexplainable, see nr_php_explain_mysql_query_is_explainable()
    */
    if ($explainable) {
        $suffix = "";
    } else {
        $suffix = ";;";
    }

    $stmt = $dbConn->prepare("SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=? AND SLEEP(?)" . $suffix);

    if (! $stmt) {
        throw new Exception("Prepare Failed : " . $dbConn->error);
    }

    $table = "STATISTICS";
    $stmt->bind_param('si', $table, $delay);
    if (! $stmt->execute()) {
        throw new Exception("Execute Failed : " . $stmt->error);
    }

    $stmt->bind_result($someId);
    $stmt->fetch();
    $stmt->close();
}

/* main code */
require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

$dbConn = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);

/* run a test where a prepared, explainable statement is prepared
   but not executed.  Then run a prepared, unexplainable slow sql
   query which should not be explained.  This will lead to the
   first stmt object being reused for the second slow sql query.

   Tests for reqgression of a bug where the query string
   from a previous, explainable SQL query with the same object ID
   ("handle") would be used in error.
*/

justPrepare($dbConn);
doSQL($dbConn, 1, false);

echo "If you see this message, it didn't crash\n";

