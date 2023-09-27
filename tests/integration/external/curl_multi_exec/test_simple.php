<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the agent instruments curl_multi_exec.
 */

/*INI
newrelic.transaction_tracer.threshold=0
newrelic.distributed_tracing_enabled = false
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "OtherTransaction/php__FILE__",
      "\u003cunknown\u003e",
      [
        [
          0, {}, {}, [
            "?? start time", "?? end time", "ROOT", "?? root attributes", [
              [
                "?? start time", "?? end time", "`0", "?? node attributes", [
                  [
		    "?? start time", "?? end time", "`1", 
                    {
                      "async_context": "`2"
                    },
		    [
                      [
                        "?? start time", "?? end time", "`3",
                        {
                          "uri": "??",
                          "library": "curl",
                          "procedure": "GET",
                          "status": "??",
                          "async_context": "`2"
                        },
                        []
                      ], 
                      [
                        "?? start time", "?? end time", "`3",
                        {
                          "uri": "??",
                          "library": "curl",
                          "procedure": "GET",
                          "status": "??",
                          "async_context": "`2"
                        },
                        []
                      ], 
                      [
                        "?? start time", "?? end time", "`3",
                        {
                          "uri": "??",
                          "library": "curl",
                          "procedure": "GET",
                          "status": "??",
                          "async_context": "`2"
                        },
                        []
                      ], 
                      [
                        "?? start time", "?? end time", "`3",
                        {
                          "uri": "??",
                          "library": "curl",
                          "procedure": "GET",
                          "status": "??",
                          "async_context": "`2"
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
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??"
            }
          }
        ],
        [
          "OtherTransaction\/php__FILE__",
          "curl_multi_exec",
          "curl_multi_exec #1",
          "External\/127.0.0.1\/all"
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

/*EXPECT
Hello world!
Hello world!
Hello world!
Hello world!
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                       ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                  ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                             ["??", "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},                      ["??", "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/




require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . $EXTERNAL_HOST;

$ch1 = curl_init();
curl_setopt($ch1, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch1, CURLOPT_RETURNTRANSFER, 1);

$ch2 = curl_init();
curl_setopt($ch2, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch2, CURLOPT_RETURNTRANSFER, 1);

$ch3 = curl_init();
curl_setopt($ch3, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch3, CURLOPT_RETURNTRANSFER, 1);

$ch4 = curl_init();
curl_setopt($ch4, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch4, CURLOPT_RETURNTRANSFER, 1);

$mh = curl_multi_init();

curl_multi_add_handle($mh, $ch1);
curl_multi_add_handle($mh, $ch2);
curl_multi_add_handle($mh, $ch3);
curl_multi_add_handle($mh, $ch4);

$active = 0;

do {
  curl_multi_exec($mh, $active);
} while ($active > 0);

echo curl_multi_getcontent($ch1) . "\n";
echo curl_multi_getcontent($ch2) . "\n";
echo curl_multi_getcontent($ch3) . "\n";
echo curl_multi_getcontent($ch4) . "\n";

curl_multi_remove_handle($mh, $ch1);
curl_multi_remove_handle($mh, $ch2);
curl_multi_remove_handle($mh, $ch3);
curl_multi_remove_handle($mh, $ch4);
curl_multi_close($mh);

