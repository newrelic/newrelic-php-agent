<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Predis provides command pipelines. In these cases, we don't know what commands
are being run, so they are instrumented as "pipeline". This test verifies
fire-and-forget pipeline functionality works correctly with Fibers.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
?>
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT_METRICS_EXIST
Datastore/Redis/all, 12
Datastore/operation/Redis/del, 1
Datastore/operation/Redis/mget, 2
Datastore/operation/Redis/exists, 3
Datastore/operation/Redis/incrby, 2
Datastore/operation/Redis/flushdb, 2
Datastore/operation/Redis/ping, 2
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction\/php__FILE__",
      "guid": "ENV[GUID_ROOT]",
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
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/test_pipeline",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
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
      "name": "Datastore\/operation\/Redis\/incrby",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/ping",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/del",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Redis\/exists",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_TEST_BASIC]",
      "span.kind": "client",
      "component": "Redis"
    },
    {},
    {
      "peer.hostname": "ENV[REDIS_HOST]",
      "peer.address": "??",
      "db.instance": "0"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_pipeline() {
  global $REDIS_HOST, $REDIS_PORT;

  env_var_for_expects("GUID_TEST_BASIC", newrelic_get_linking_metadata()['span.id'] ?? '');

  $client = new Predis\Client(array(
    'host' => $REDIS_HOST,
    'port' => $REDIS_PORT,
  ));

  try {
      $client->connect();
  } catch (Exception $e) {
      die("skip: " . $e->getMessage() . "\n");
  }

  /* Generate a unique key to use for this test run. */
  $key = randstr(16);
  if ($client->exists($key)) {
      echo "key already exists: $key\n";
      exit(1);
  }

  /* method 1 with fiber suspension */
  $replies = $client->pipeline(array('fire-and-forget'), function($pipe) {
    global $key;
    $pipe->ping();
    Fiber::suspend();
    $pipe->flushdb();
    $pipe->incrby($key, 7);
    $pipe->exists($key);
    $pipe->mget('does_not_exist', $key);
  });

  /* method 2 with fiber suspension */
  $pipe = $client->pipeline(array('fire-and-forget'));
  $pipe->ping();
  Fiber::suspend();
  $pipe->flushdb();
  $pipe->incrby($key, 13);
  $pipe->exists($key);
  $pipe->mget('does_not_exist', $key);
  $replies = $pipe->execute();

  $client->del($key);
  $client->quit();
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$fiber = new Fiber('test_pipeline');
$fiber->start();
$fiber->resume();
$fiber->resume();
