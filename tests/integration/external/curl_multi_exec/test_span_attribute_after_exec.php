<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A span attribute added after the transfers of a curl multi handle have finished
SHALL be set on the span that is current in the caller, and not on the
curl_multi_exec span.
*/

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
*/

/*EXPECT
Hello world!
*/

/*EXPECT_SPAN_EVENTS_LIKE
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
    {
      "after_multi": 1
    },
    {}
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "curl_multi_exec",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {},
    {}
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$ch = curl_init();
curl_setopt($ch, CURLOPT_URL, $EXTERNAL_HOST);
curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);

$mh = curl_multi_init();
curl_multi_add_handle($mh, $ch);

$active = 0;

do {
  curl_multi_exec($mh, $active);
} while ($active > 0);

echo curl_multi_getcontent($ch) . "\n";
curl_multi_remove_handle($mh, $ch);
curl_multi_close($mh);

newrelic_add_custom_span_parameter('after_multi', 1);
