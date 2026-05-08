<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report an error and cause no additional harm when
the bind-in-execute parameters are mismatched
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
Fatal error: Uncaught ValueError: mysqli_stmt_execute.*
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "/Uncaught exception 'ValueError' with message 'mysqli_stmt_execute.* must consist of exactly 1 elements, 2 present.*./",
      "ValueError",
      {
        "stack_trace":[
          " in mysqli_stmt_execute called at __FILE__ (??)",
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

require_once(realpath(dirname(__FILE__)) . '/mysqli.inc');

function test_stmt_execute($link, array $data)
{
    $query = "SELECT Name FROM myCity WHERE CountryCode IN (?)";
    $stmt = mysqli_prepare($link, $query);
    if (false === $stmt) {
        echo mysqli_error($link) . "\n";
        return;
    }
    if (false === mysqli_stmt_execute($stmt, $data)) {
        echo mysqli_stmt_error($stmt) . "\n";
        return;
    }

    if (false === mysqli_stmt_bind_result($stmt, $value)) {
        echo mysqli_stmt_error($stmt) . "\n";
        return;
    }

    while (mysqli_stmt_fetch($stmt)) {
        echo $value . "\n";
    }

    mysqli_stmt_close($stmt);
}

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
    echo mysqli_connect_error() . "\n";
    exit(1);
}

$link->query("CREATE TABLE IF NOT EXISTS myCity(Name VARCHAR(100) NOT NULL, CountryCode VARCHAR(40) NOT NULL, District VARCHAR(100) NOT NULL)");
$query = "INSERT INTO myCity (Name, CountryCode, District) VALUES (?,?,?)";
$stmt = mysqli_prepare($link, $query);
$data = ['Stuttgart', 'DEU', 'Baden-Wuerttemberg'];
mysqli_stmt_execute($stmt, $data);
$stmt = mysqli_prepare($link, $query);
$data = ['Austin', 'USA', 'Travis'];
mysqli_stmt_execute($stmt, $data);
$stmt = mysqli_prepare($link, $query);
$data = ['Miami', 'USA', 'Dade'];
mysqli_stmt_execute($stmt, $data);
$stmt = mysqli_prepare($link, $query);
$data = ['Houston', 'USA', 'Harris'];
mysqli_stmt_execute($stmt, $data);

test_stmt_execute($link, ['DEU', 'USA']);
mysqli_close($link);
