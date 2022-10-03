<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test adding curl handle while curl multi exec is in flight.
 */

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.threshold = 0
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
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
                    "?? start time", "?? end time", "`1", {"async_context": "`2"}, [
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
              "cpu_sys_time": "??",
              "guid": "??",
              "sampled": "??",
              "priority": "??",
              "traceId": "??"
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
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                           [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all",
      "scope":"OtherTransaction/php__FILE__"},               [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Create/Success"},  [3, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/CreatePayload/Success"},
                                                             [3, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function test_curl_multi_exec_add_handles() {
  $url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

  $ch1 = curl_init($url);
  $ch2 = curl_init($url);
  $ch3 = curl_init($url);
  $mh = curl_multi_init();

  curl_multi_add_handle($mh, $ch1);
  curl_multi_add_handle($mh, $ch2);

  $active = 0;
  curl_multi_exec($mh, $active);
  curl_multi_add_handle($mh, $ch3);

  do {
    curl_multi_exec($mh, $active);
  } while ($active > 0);

  curl_close($ch1);
  curl_close($ch2);
  curl_close($ch3);
  curl_multi_close($mh);
}

test_curl_multi_exec_add_handles();
