<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for mysqli::prepare.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.transaction_tracer.explain_enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
ok - execute select 3*0
ok - iteration  0
ok - execute select 3*1
ok - iteration  1
ok - execute select 3*2
ok - iteration  2
ok - execute select 3*3
ok - iteration  3
ok - execute select 3*4
ok - iteration  4
ok - execute select 3*5
ok - iteration  5
ok - execute select 3*6
ok - iteration  6
ok - execute select 3*7
ok - iteration  7
ok - execute select 3*8
ok - iteration  8
ok - execute select 3*9
ok - iteration  9
ok - execute select 3*10
ok - iteration 10
ok - execute select 3*11
ok - iteration 11
ok - execute select 3*12
ok - iteration 12
ok - execute select 3*13
ok - iteration 13
ok - execute select 3*14
ok - iteration 14
ok - execute select 3*15
ok - iteration 15
ok - execute select 3*16
ok - iteration 16
ok - execute select 3*17
ok - iteration 17
ok - execute select 3*18
ok - iteration 18
ok - execute select 3*19
ok - iteration 19
ok - execute select 3*20
ok - iteration 20
ok - execute select 3*21
ok - iteration 21
ok - execute select 3*22
ok - iteration 22
ok - execute select 3*23
ok - iteration 23
ok - execute select 3*24
ok - iteration 24
ok - execute select 3*25
ok - iteration 25
ok - execute select 3*26
ok - iteration 26
ok - execute select 3*27
ok - iteration 27
ok - execute select 3*28
ok - iteration 28
ok - execute select 3*29
ok - iteration 29
ok - execute select 3*30
ok - iteration 30
ok - execute select 3*31
ok - iteration 31
ok - execute select 3*32
ok - iteration 32
ok - execute select 3*33
ok - iteration 33
ok - execute select 3*34
ok - iteration 34
ok - execute select 3*35
ok - iteration 35
ok - execute select 3*36
ok - iteration 36
ok - execute select 3*37
ok - iteration 37
ok - execute select 3*38
ok - iteration 38
ok - execute select 3*39
ok - iteration 39
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                         [40, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                    [40, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/all"},                   [40, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/MySQL/allOther"},              [40, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select"},      [40, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/MySQL/select",
      "scope":"OtherTransaction/php__FILE__"},         [40, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                  [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},          [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},             [ 1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"}, [ 1, "??", "??", "??", "??", "??"]]
  ]
]*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/mysqli.inc');

function test_prepare_oo ()
{
  global $MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET;

  /*
   * Iterate enough times to exceed C macro
   * NR_PREPARED_STATEMENT_ALLOCATION_INCREMENT, at 16.
   */
  $N = 40;
  $queries = array ($N);
  $mysqlis = array ($N);

  for ($i = 0; $i < $N; $i++) {
    /*
     * Because of how php_instrument.c attempts to save obj's that
     * hold mysqli objects we have to have a unique object for each mysqli
     * object here in this test.
     */
    $query = "select 3*" . $i;
    $queries[$i] = $query;

    $mysqlis[$i] = new mysqli ($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET);
    if (mysqli_connect_errno ()) {
      printf ("Connect failed: %s\n", mysqli_connect_error ());
      exit ();
    }

    /*
     * Note: Sometimes the prepare doesn't always work.
     */
    if ($stmt = $mysqlis[$i]->prepare ($queries[$i])) {
      $execute_result = $stmt->execute ();
      tap_assert ($execute_result, sprintf ("execute %s", $queries[$i]));
      if ($execute_result) {
        $stmt->bind_result ($value);
        $stmt->fetch ();
        tap_equal (3*$i, $value, sprintf("iteration %2d", $i));
      }
    }
  }

  for ($i = 0; $i < $N; $i++) {
    $mysqlis[$i]->close ();
  }
}

test_prepare_oo ();
