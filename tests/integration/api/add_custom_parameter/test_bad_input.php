<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests how the agent converts custom parameter values to strings.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT
ok - should reject zero args
ok - should reject one arg
ok - should reject more than two args
ok - should reject infinity
ok - should reject NaN
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
        "error": false
      },
      {
      },
      {
      }
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

function test_add_custom_parameter() {
  $result = @newrelic_add_custom_parameter();
  tap_refute($result, "should reject zero args");

  $result = @newrelic_add_custom_parameter("key1");
  tap_refute($result, "should reject one arg");

  /*
   * In PHP 8.0 this now throws an ArgumentCountError instead of a warning.
   * Lets catch the error and move on to allow for backwards compatibility.
   */
  try {
    $result = @newrelic_add_custom_parameter("key2", "value", "bad");
    tap_refute($result, "should reject more than two args");
  } catch (\ArgumentCountError $e) {
    echo 'ok - should reject more than two args',"\n";
  }

  $result = @newrelic_add_custom_parameter("key3", INF);
  tap_refute($result, "should reject infinity");

  $result = @newrelic_add_custom_parameter("key4", NAN);
  tap_refute($result, "should reject NaN");
}

test_add_custom_parameter();
