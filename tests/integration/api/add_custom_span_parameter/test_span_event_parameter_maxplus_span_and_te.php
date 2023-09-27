<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that attributes only a maximum of 64 custom attributes are added to span events.  Custom span attributes take precedence on the span before transaction events are added.
.*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.0", "<")) {
  die("skip: CLM for PHP 5 not supported\n");
}
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = false
newrelic.span_events_enabled=1
newrelic.cross_application_tracer.enabled = false
newrelic.loglevel = "verbosedebug"
*/

/*EXPECT
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute NOT added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
ok - string attribute added
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
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      {
        "64": 64,
        "63": 63,
        "62": 62,
        "61": 61,
        "60": 60,
        "59": 59,
        "58": 58,
        "57": 57,
        "56": 56,
        "55": 55,
        "54": 54,
        "53": 53,
        "52": 52,
        "51": 51,
        "50": 50,
        "49": 49,
        "48": 48,
        "47": 47,
        "46": 46,
        "45": 45,
        "44": 44,
        "43": 43,
        "42": 42,
        "41": 41,
        "40": 40,
        "39": 39,
        "38": 38,
        "37": 37,
        "36": 36,
        "35": 35,
        "34": 34,
        "33": 33,
        "32": 32,
        "31": 31,
        "30": 30,
        "29": 29,
        "28": 28,
        "27": 27,
        "26": 26,
        "25": 25,
        "24": 24,
        "23": 23,
        "22": 22,
        "21": 21,
        "20": 20,
        "19": 19,
        "18": 18,
        "17": 17,
        "16": 16,
        "15": 15,
        "14": 14,
        "13": 13,
        "12": 12,
        "11": 11,
        "10": 10,
        "9": 9,
        "8": 8,
        "7": 7,
        "6": 6,
        "5": 5,
        "4": 4,
        "3": 3,
        "2": 2,
        "1": 1
      },
      {
      }
    ]
  ]
]
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 2
  },
  [
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "OtherTransaction\/php__FILE__",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "sampled": true,
        "nr.entryPoint": true,
        "timestamp": "??",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {}
    ],
    [
      {
        "type": "Span",
        "traceId": "??",
        "transactionId": "??",
        "sampled": true,
        "priority": "??",
        "name": "Custom\/a",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
      },
      {
              "134": 134,
        "133": 133,
        "132": 132,
        "131": 131,
        "130": 130,
        "129": 129,
        "128": 128,
        "127": 127,
        "126": 126,
        "125": 125,
        "124": 124,
        "123": 123,
        "122": 122,
        "121": 121,
        "120": 120,
        "119": 119,
        "118": 118,
        "117": 117,
        "116": 116,
        "115": 115,
        "114": 114,
        "113": 113,
        "112": 112,
        "111": 111,
        "110": 110,
        "109": 109,
        "108": 108,
        "107": 107,
        "106": 106,
        "105": 105,
        "104": 104,
        "103": 103,
        "102": 102,
        "101": 101,
        "64": 64,
        "63": 63,
        "62": 62,
        "61": 61,
        "60": 60,
        "59": 59,
        "58": 58,
        "57": 57,
        "56": 56,
        "55": 55,
        "54": 54,
        "53": 53,
        "52": 52,
        "51": 51,
        "50": 50,
        "49": 49,
        "48": 48,
        "47": 47,
        "46": 46,
        "45": 45,
        "44": 44,
        "43": 43,
        "42": 42,
        "41": 41,
        "40": 40,
        "39": 39,
        "38": 38,
        "37": 37,
        "36": 36,
        "35": 35
      },
      {
        "code.lineno": 367,
        "code.filepath": "__FILE__",
        "code.function": "a"
      }
    ]
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"Custom/a"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Custom/a",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/add_custom_tracer"},               [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_parameter"},            [65, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/add_custom_span_parameter"},       [34, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]]
  ]
]
*/



require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');

newrelic_add_custom_tracer("a");

function a() {
  for ($i = 1; $i <= 64; $i++) {
    tap_assert(newrelic_add_custom_parameter($i, $i), "string attribute added");
  }
  tap_refute(newrelic_add_custom_parameter($i, $i), "string attribute NOT added");
  for ($i = 101; $i <= 134; $i++) {
    tap_assert(newrelic_add_custom_span_parameter($i, $i), "string attribute added");
  }
}
a();
