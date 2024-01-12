<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should add transaction GUID to error traces, even if CAT and DT as disabled.
*/

/*SKIPIF
<?php
if (!function_exists('newrelic_get_error_json')) {
  die("warn: release builds of the agent do not include newrelic_get_error_json()");
}
if (!function_exists('newrelic_get_transaction_guid')) {
  die("warn: release builds of the agent do not include newrelic_get_transaction_guid()");
}
*/

/*INI
newrelic.distributed_tracing_enabled=false
newrelic.cross_application_tracer.enabled=false
*/

/*EXPECT_TRACED_ERRORS
[
  "?? agent run id",
  [
    [
      "?? when",
      "OtherTransaction/php__FILE__",
      "Noticed exception 'RuntimeException' with message 'Hi!' in __FILE__:??",
      "RuntimeException",
      {
        "stack_trace": [
          " in throw_it called at __FILE__ (??)"
        ],
        "agentAttributes": "??",
        "intrinsics": "??"
      },
      "?? txn guid"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');

function throw_it() {
  newrelic_notice_error(new RuntimeException('Hi!'));
}

throw_it();

/* capture error trace json and verify transaction GUID */
$json = newrelic_get_error_json();
$payload = json_decode($json, true);
$guid = newrelic_get_transaction_guid();
tap_equal($guid, $payload[5], "Error trace includes txn guid");