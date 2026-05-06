<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report an error and cause no additional problems when passing an
invalid binding parameter
*/

/*SKIPIF
<?php require("skipif.inc");
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1 not supported\n");
}
*/


/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT_REGEX
Fatal error: Uncaught TypeError: mysqli_stmt::execute.*
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "/Uncaught exception 'TypeError' with message 'mysqli_stmt::execute.*./",
      "TypeError",
      {
        "stack_trace":[
          " in mysqli_stmt::execute called at __FILE__ (??)",
          " in test_stmt_execute called at __FILE__ (??)"
        ],
        "agentAttributes":{},
        "intrinsics": "??"
      },
      "??"
    ]
  ]
]

*/

require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_stmt_execute($mysqli, $data)
{
  $query = "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name=?";
  $stmt = $mysqli->prepare($query);

  if (FALSE === $stmt->execute($data) ||
      FALSE === $stmt->bind_result($name)) {
    echo $stmt->error . "\n";
    $stmt->close();
    return;
  }

  try {
    $stmt->fetch();
  } catch (mysqli_sql_exception $e) {
    echo (string)$e;
  }

  $stmt->close();
}

$mysqli = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_stmt_execute($mysqli, 7);
$mysqli->close();
