<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
Code level metrics should be added to nested functions; however,
CLM should NOT be added to functions that are so short they will
normally be ignored.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
*/

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.code_level_metrics.enabled = 1
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 3
  },
  [
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "code.lineno": 1,
        "code.filepath": "__FILE__",
        "code.function": "__FILE__"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/level_2",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 228,
        "code.filepath": "__FILE__",
        "code.function": "level_2"
      }
    ],
    [
      {
        "category": "generic",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Custom\/level_1",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??"
      },
      {},
      {
        "code.lineno": 224,
        "code.filepath": "__FILE__",
        "code.function": "level_1"
      }
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction\/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      {},
      {
        "code.lineno": 1,
        "code.filepath": "__FILE__",
        "code.function": "__FILE__"
      }
    ]
  ]
]

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
          0,
          {},
          {},
          [
            "?? start time", "?? end time", "ROOT", "?? root attributes",
            [
              [
                "?? start time", "?? end time", "`0", "?? node attributes",
                [
                  [
                    "?? start time", "?? end time", "`1",
                            {
                              "code.lineno": 228,
                              "code.filepath": "__FILE__",
                              "code.function": "level_2"
                            },
                    [
                      [
                        "?? start time", "?? end time", "`2",
                            {
                              "code.lineno": 224,
                              "code.filepath": "__FILE__",
                              "code.function": "level_1"
                        },
                        []
                      ]
                    ]
                  ]
                ]
              ]
            ]
          ],
          {
            "agentAttributes": {
              "code.lineno": 1,
              "code.filepath": "__FILE__",
              "code.function": "__FILE__"
            },
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??",
              "guid": "??",
              "sampled": true,
              "priority": "??",
              "traceId": "??"
            }
          }
        ],
        [
          "OtherTransaction\/php__FILE__",
          "Custom\/level_2",
          "Custom\/level_1"
        ]
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      null
    ]
  ]
]
*/

/*
 * Normally super short duration functions are ignored.
 * We'll force some to be noticed, while others should
 * contrinue to be ignored.
 */
newrelic_add_custom_tracer("level_1");
newrelic_add_custom_tracer("level_2");

function level_0() {
    echo "level_0\n";
}

function level_1() {
    level_0();
}

function level_2() {
    level_1();
}

level_2();
