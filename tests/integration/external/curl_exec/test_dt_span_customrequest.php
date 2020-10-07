<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that DT and span events work correctly. This also tests that curl_setopt
does not crash when CURLOPT_CUSTOMREQUEST/CURLOPT_HTTPHEADER are called with
unexpected types.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = 0
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
if (version_compare(PHP_VERSION, "7.4", ">=")) {
  die("skip: PHP >= 7.4.0: passing ArrayObject to curl_setopt doesn't work\n");
}
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
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
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "External\/127.0.0.1\/all",
        "guid": "??",
        "type": "Span",
        "category": "http",
        "priority": "??",
        "sampled": true,
        "timestamp": "??",
        "parentId": "??",
        "span.kind": "client",
        "component": "curl"
      },
      {},
      {
        "http.url": "??",
        "http.method": "GET",
        "http.statusCode": 200
      }
    ]
  ]
]
*/

/*EXPECT_REGEX
traceparent=found tracestate=found newrelic=found X-NewRelic-ID=missing X-NewRelic-Transaction=missing tracing endpoint reached
ok - tracing successful
ok - header set successfully
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$ch = curl_init($url);
curl_setopt($ch, CURLINFO_HEADER_OUT, true);

// This is expecting a string not a number
curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 12);

curl_setopt($ch, CURLOPT_CUSTOMREQUEST, "GET");

// Invalid object
$object = new stdClass();
curl_setopt($ch, CURLOPT_HTTPHEADER, $object);

// Valid object
$object = new \ArrayObject();
$object[] = 'Test-Header: do you see me?';
curl_setopt($ch, CURLOPT_HTTPHEADER, $object);

$result = curl_exec($ch);
tap_not_equal(false, $result, "tracing successful");

$headerSent = curl_getinfo($ch, CURLINFO_HEADER_OUT);
tap_not_equal(false, strpos($headerSent, "do you see me"), "header set successfully");

curl_close($ch);
