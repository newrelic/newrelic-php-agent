<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
If an inbound payload was received, the root span event must use the "id" 
value of the inbound payload as its parent id;
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability_trusted"])) {
    die("skip: env vars required");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
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
        "parentId" : "4321",
        "transaction.name": "OtherTransaction\/php__FILE__"
      },
      {},
      {
        "parent.type": "??",
        "parent.app": "??",
        "parent.account": "??",
        "parent.transportType": "??",
        "parent.transportDuration": "??"
      }
    ],
    [
      {
        "traceId": "??",
        "duration": "??",
        "transactionId": "??",
        "name": "Custom\/main",
        "guid": "??",
        "type": "Span",
        "category": "generic",
        "priority": "??",
        "parentId": "??",
        "sampled": true,
        "timestamp": "??"
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT
Hello
*/

$payload = "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"111111\",\"ap\":\"222222\",\"tr\":\"3221bf09aa0bcf0d\",\"tx\":\"6789\",\"id\":\"4321\",\"tk\":\"1010\",\"pr\":0.1234,\"sa\":true,\"ti\":1482959525577,\"tk\":\"{$_ENV['ACCOUNT_supportability_trusted']}\"}}";
newrelic_accept_distributed_trace_payload($payload, 'http');
newrelic_add_custom_tracer('main'); 

function main()
{
  echo 'Hello';
}
main();

