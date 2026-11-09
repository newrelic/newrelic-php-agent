<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report an error and cause no additional harm when the
mysqli_execute_query() parameters do not match the placeholders.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (version_compare(PHP_VERSION, "8.2", "<")) {
  die("skip: mysqli_execute_query() exists from PHP 8.2\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "??",
      "OtherTransaction\/php__FILE__",
      "/Uncaught exception 'ValueError' with message 'mysqli_execute_query.* exactly 1 element.*./",
      "ValueError",
      {
        "stack_trace":[
          " in mysqli_execute_query called at __FILE__ (??)",
          " in test_execute_query called at __FILE__ (??)"
        ],
        "agentAttributes":{},
        "intrinsics": "??"
      },
      "??"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function test_execute_query($link)
{
  mysqli_execute_query($link, "SELECT TABLE_NAME FROM information_schema.tables WHERE table_name = ?", ['STATISTICS', 'extra']);
}

$link = new mysqli($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

test_execute_query($link);
