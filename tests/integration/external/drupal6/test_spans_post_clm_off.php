<?php

/*DESCRIPTION
Span events are generated and external span event fields are set correctly.
*/

// force the framework to avoid requiring the drupal detection file
/*INI
newrelic.framework = drupal
newrelic.distributed_tracing_enabled = true
newrelic.transaction_tracer.threshold = 0
newrelic.transaction_tracer.detail = 0
newrelic.code_level_metrics.enabled=false
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "7.4", ">")) {
  die("skip: PHP > 7.4.0 not supported\n");
}
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
        "name": "Custom\/drupal_http_request",
        "guid": "??",
        "timestamp": "??",
        "duration": "??",
        "category": "generic",
        "parentId": "??"
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
        "component": "Drupal"
      },
      {},
      {
        "http.url": "??",
        "http.method": "POST",
        "http.statusCode": 200
      }
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/drupal_6_bootstrap.inc');
require_once(realpath(dirname(__FILE__)) . '/drupal_6_common.inc');
require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');
$rv =drupal_http_request($url, NULL, "POST");
