<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
GH #1280 regression test: https://github.com/newrelic/newrelic-php-agent/issues/1280

Under opcache.preload, a library signature file executes exactly once, at
server startup, before any transaction exists - and, because the class it
declares is now preloaded, is never require()'d again for the life of the
pool. Library detection can't rely on that one require() at file-execution
time, so the agent re-derives it every transaction from opcache_get_status()
information, matching each script's path against the known signature files
without re-executing the file.

This test uses a fake MongoDB\Operation\Aggregate class (matching the libraries[]
path suffix mongodb/src/client.php) purely as a vehicle for that preloaded-path
match - no real mongodb extension or package is required. Because the MongoDB
mock doesn't provide a real database name or connection/instance info, this
test disables database name and instance reporting.

Furthermore, a preload file used in this test executes when the agent is not recording,
therefore preload file execution is not captured as a separate analytic event and
additional root span with the name OtherTransaction/php/<unknown> is not created.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
if (!extension_loaded('Zend OPcache')) {
  die("skip: OPcache extension required\n");
}
*/

/*INI
opcache.enable=1
opcache.enable_cli=1
opcache.preload=config/ignored_preload.php
opcache.preload_user=www-data
;comment=disabling database name and instance reporting:
;comment=mongodb mock used for this test doesn't mock these features
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_METRICS_EXIST
Supportability/library/MongoDB/detected, 1
Datastore/operation/MongoDB/aggregate, 1
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
      {}
    ],
    [
      {
        "category": "datastore",
        "type": "Span",
        "guid": "??",
        "traceId": "??",
        "transactionId": "??",
        "name": "Datastore\/operation\/MongoDB\/aggregate",
        "timestamp": "??",
        "duration": "??",
        "priority": "??",
        "sampled": true,
        "parentId": "??",
        "span.kind": "client",
        "component": "MongoDB"
      },
      {},
      {
        "peer.address": "unknown:unknown"
      }
    ]
  ]
]
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 50,
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
        "databaseDuration": "??",
        "databaseCallCount": 1,
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      {},
      {}
    ]
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

// Trigger MongoDB's auto-instrumentation, which executes as expected
// even though the mongodb signature file was not explicitly require()'d
$op = new \MongoDB\Operation\Aggregate();
$op->execute();
