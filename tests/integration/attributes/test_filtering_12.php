<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test for filtering:
Test 12: 
   include = "*"
   exclude = "BB"
   input = "AA" "AB" "AC" "BB"
   expect = "AA" "AB" "AC"
*/

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.special.expensive_node_min = 0
newrelic.attributes.enabled = 1
newrelic.attributes.include = "*"
newrelic.attributes.exclude = "BB"
*/

/*SKIPIF
<?php
die("disabled for now");
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "<unknown>",
      [
        [
          0, {}, {},
          "?? trace details",
          {
            "userAttributes": {
              "AC": "AC",
              "AB": "AB",
              "AA": "AA"
            },
            "intrinsics": "??"
          }
        ],
        [
          "OtherTransaction/php__FILE__",
          "Custom/force_transaction_trace"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      "?? synthetics resource id"
    ]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');

force_transaction_trace();

newrelic_add_custom_parameter("AA", "AA");
newrelic_add_custom_parameter("AB", "AB");
newrelic_add_custom_parameter("AC", "AC");
newrelic_add_custom_parameter("BB", "BB");

