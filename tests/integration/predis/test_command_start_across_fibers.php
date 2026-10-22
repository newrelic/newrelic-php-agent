<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A Predis command written in one fiber and read in another SHALL be reported
once, with the fiber that reads the response as its parent.

The agent keeps the start time of a written command with the command object,
not with the fiber. The first fiber writes a command and is suspended. The
second fiber reads the response of that command object.
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
ok - response read in another fiber
ok - the reading fiber has the get segment
ok - the writing fiber has no get segment
ok - the main function has no get segment
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
      "parentId": "ENV[GUID_B]",
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

/* The number of children with the given name that the segment(s) of a function have. */
function count_children($txn, $function, $name) {
  $count = 0;
  foreach ($txn->getTrace()->findSegmentsByName('Custom/' . $function) as $segment) {
    foreach ($segment->children as $child) {
      if ($name === $child->name) {
        $count++;
      }
    }
  }
  return $count;
}

function writing_fiber($connection, $command) {
  env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
  $connection->writeRequest($command);
  /* The agent skips a segment that starts and stops at the same time, so give the command some time. */
  usleep(2000);
  Fiber::suspend();
}

function reading_fiber($connection, $command) {
  env_var_for_expects("GUID_B", newrelic_get_linking_metadata()['span.id'] ?? '');
  tap_equal(null, $connection->readResponse($command), 'response read in another fiber');
}

function test_read_in_another_fiber() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }
  $connection = $client->getConnection();
  $command = $client->createCommand('get', array(randstr(16)));

  $writer = new Fiber('writing_fiber');
  $writer->start($connection, $command);

  $reader = new Fiber('reading_fiber');
  $reader->start($connection, $command);

  $writer->resume();
  $client->disconnect();
}

test_read_in_another_fiber();

$txn = new Transaction;
$get = 'Datastore/operation/Redis/get';
tap_equal(1, count_children($txn, 'reading_fiber', $get), 'the reading fiber has the get segment');
tap_equal(0, count_children($txn, 'writing_fiber', $get), 'the writing fiber has no get segment');
tap_equal(0, count_children($txn, 'test_read_in_another_fiber', $get), 'the main function has no get segment');
