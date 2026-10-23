<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Two fibers that write a Predis command each and are suspended SHALL each report
their own command, with the fiber that reads the response as its parent.

The fibers share one connection. The first fiber writes a command and is
suspended, the second writes another and is suspended, and then each is resumed
and reads the response of its own command. The agent keeps the start time of
each command with its command object, so each response finds the start time of
its own write.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../../include/config.php');
require('predis.inc');
if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: PHP < 8.1.0 not supported\n");
}
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - first response read
ok - second response read
ok - the first fiber has its get segment
ok - the second fiber has its get segment
ok - the trace has no other get segment
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Redis/get, 2
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore/operation/Redis/get",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
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
      "name": "Datastore/operation/Redis/get",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??",
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
require_once(__DIR__.'/../../include/integration.php');
require_once(__DIR__.'/predis.inc');

use NewRelic\Integration\Transaction;

/* The number of segments with the given name below any of a function's segments, at any depth. */
function count_descendants($segment, $name) {
  $count = 0;
  foreach ($segment->children as $child) {
    if ($name === $child->name) {
      $count++;
    }
    $count += count_descendants($child, $name);
  }
  return $count;
}

function count_below($txn, $function, $name) {
  $count = 0;
  foreach ($txn->getTrace()->findSegmentsByName('Custom/' . $function) as $segment) {
    $count += count_descendants($segment, $name);
  }
  return $count;
}
function fiber_a($client, $connection, $command) {
  env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
  $connection->writeRequest($command);
  /* The agent skips a segment that starts and stops at the same time, so give the command some time. */
  usleep(2000);
  Fiber::suspend();
  tap_equal(null, $connection->readResponse($command), 'first response read');
}

function fiber_b($client, $connection, $command) {
  env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
  $connection->writeRequest($command);
  /* The agent skips a segment that starts and stops at the same time, so give the command some time. */
  usleep(2000);
  Fiber::suspend();
  tap_equal(null, $connection->readResponse($command), 'second response read');
}

function test_interleaved() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }
  $connection = $client->getConnection();

  $a = new Fiber('fiber_a');
  $b = new Fiber('fiber_b');
  $a->start($client, $connection, $client->createCommand('get', array(randstr(16))));
  $b->start($client, $connection, $client->createCommand('get', array(randstr(16))));

  /* The responses arrive in the order the commands were written. */
  $a->resume();
  $b->resume();
  $client->disconnect();
}

test_interleaved();

$txn = new Transaction;
$get = 'Datastore/operation/Redis/get';
tap_equal(1, count_below($txn, 'fiber_a', $get), 'the first fiber has its get segment');
tap_equal(1, count_below($txn, 'fiber_b', $get), 'the second fiber has its get segment');
tap_equal(2, iterator_count($txn->getTrace()->findSegmentsByName($get)), 'the trace has no other get segment');
