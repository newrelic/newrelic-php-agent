<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A Predis command that a fiber writes and frees without reading its response
SHALL leave nothing for a later command object that takes its handle, and SHALL
NOT disturb the pending command of another fiber.

The first fiber writes a command and is suspended, and the second writes another
and is suspended. The first fiber is resumed and frees its command. A new
command object in the main fiber takes the freed handle and reads the pending
response of the first command with no writeRequest() of its own, and is not
reported. The second fiber is then resumed and reads its response, which is
reported once with the second fiber as its parent.
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
ok - a command object reuses the handle
ok - response read in the main fiber
ok - response read in the second fiber
ok - the second fiber has its get segment
ok - the first fiber has no get segment
ok - the trace has no other get segment
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Redis/get, 1
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
function freeing_fiber($client, $connection) {
  global $freed_handle;
  env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
  $command = $client->createCommand('get', array(randstr(16)));
  $connection->writeRequest($command);
  $freed_handle = spl_object_id($command);
  Fiber::suspend();
  unset($command);
}

function pending_fiber($client, $connection) {
  env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
  $command = $client->createCommand('get', array(randstr(16)));
  $connection->writeRequest($command);
  /* The agent skips a segment that starts and stops at the same time, so give the command some time. */
  usleep(2000);
  Fiber::suspend();
  tap_equal(null, $connection->readResponse($command), 'response read in the second fiber');
}

function test_freed_in_fiber() {
  global $REDIS_HOST, $REDIS_PORT, $freed_handle;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }
  $connection = $client->getConnection();

  $a = new Fiber('freeing_fiber');
  $b = new Fiber('pending_fiber');
  $a->start($client, $connection);
  $b->start($client, $connection);

  $a->resume();

  /* Other handles can be freed after the one that matters. */
  $candidates = array();
  for ($i = 0; $i < 8; $i++) {
    $candidates[] = $client->createCommand('get', array(randstr(16)));
  }
  $position = array_search($freed_handle, array_map('spl_object_id', $candidates));
  tap_assert(false !== $position, 'a command object reuses the handle');

  /* The first command's response is the first one on the connection. */
  tap_equal(null, $connection->readResponse($candidates[$position]), 'response read in the main fiber');

  $b->resume();
  $client->disconnect();
}

test_freed_in_fiber();

$txn = new Transaction;
$get = 'Datastore/operation/Redis/get';
tap_equal(1, count_below($txn, 'pending_fiber', $get), 'the second fiber has its get segment');
tap_equal(0, count_below($txn, 'freeing_fiber', $get), 'the first fiber has no get segment');
tap_equal(1, iterator_count($txn->getTrace()->findSegmentsByName($get)), 'the trace has no other get segment');
