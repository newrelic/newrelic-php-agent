<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore metrics for Redis basic operations if fibers are involved.
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

/*EXPECT
ok - set key
ok - get key
Starting Func 'a'
ok - delete key
ok - delete missing key
ok - reuse deleted key
Ending Func 'a'
ok - set duplicate key
ok - delete key
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
      "name": "Custom\/test_basic",
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
      "name": "Datastore\/operation\/Redis\/set",
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
      "name": "Datastore\/operation\/Redis\/setnx",
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
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/a",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
    },
    {},
    {}
  ]
]
*/

/*EXPECT_METRICS_EXIST
Datastore/Redis/all, 8
Datastore/operation/Redis/del, 3
Datastore/operation/Redis/get, 1
Datastore/operation/Redis/exists, 1
Datastore/operation/Redis/set, 1
Datastore/operation/Redis/setnx, 2
*/

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/../../include/integration.php');
require_once(__DIR__.'/predis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  env_var_for_expects("GUID_TEST_BASIC", newrelic_get_linking_metadata()['span.id'] ?? '');

  try {
      $client->connect();
  } catch (Exception $e) {
      die("skip: " . $e->getMessage() . "\n");
  }

  /* Generate a unique key to use for this test run */
  $key = randstr(16);
  if ($client->exists($key)) {
      echo "key already exists: $key\n";
      exit(1);
  }

  /* The tests */
  $rval = $client->set($key, 'bar');
  tap_equal('OK', $rval->getPayload(), 'set key');
  tap_equal('bar', $client->get($key), 'get key');
  Fiber::suspend();
  tap_equal(1, $client->del($key), 'delete key');
  tap_equal(0, $client->del($key), 'delete missing key');

  tap_assert($client->setnx($key, 'bar') == 1, 'reuse deleted key');
  Fiber::suspend();
  tap_refute($client->setnx($key, 'bar') == 1, 'set duplicate key');

  /* Cleanup the key used by this test run */
  tap_equal(1, $client->del($key), 'delete key');

  /* Close connection */
  $client->disconnect();
}

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$fiber_a = new Fiber('a');
$fiber_predis = new Fiber('test_basic');

$fiber_predis->start();
$fiber_a->start();
$fiber_predis->resume();
$fiber_a->resume();
$fiber_predis->resume();
