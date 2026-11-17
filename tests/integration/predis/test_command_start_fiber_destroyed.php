<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A Predis command that is written in a fiber which is then destroyed while
suspended SHALL leave nothing for a later command object that takes its handle.

The fiber writes a command and is suspended. It is destroyed without being
resumed, which frees the command. A new command object takes the freed handle
and reads the pending response with no writeRequest() of its own. Neither
command is reported.
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
ok - response read
ok - the destroyed fiber has no get segment
ok - the main function has no get segment
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/all
Datastore/operation/Redis/get
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
function writing_fiber($client, $connection) {
  global $written_handle;
  $command = $client->createCommand('get', array(randstr(16)));
  $connection->writeRequest($command);
  $written_handle = spl_object_id($command);
  Fiber::suspend();
}

function test_fiber_destroyed() {
  global $REDIS_HOST, $REDIS_PORT, $written_handle;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }
  $connection = $client->getConnection();

  $fiber = new Fiber('writing_fiber');
  $fiber->start($client, $connection);
  unset($fiber);

  $candidates = array();
  for ($i = 0; $i < 8; $i++) {
    $candidates[] = $client->createCommand('get', array(randstr(16)));
  }
  $position = array_search($written_handle, array_map('spl_object_id', $candidates));
  tap_assert(false !== $position, 'a command object reuses the handle');

  tap_equal(null, $connection->readResponse($candidates[$position]), 'response read');
  $client->disconnect();
}

test_fiber_destroyed();

$txn = new Transaction;
$get = 'Datastore/operation/Redis/get';
tap_equal(0, count_children($txn, 'writing_fiber', $get), 'the destroyed fiber has no get segment');
tap_equal(0, count_children($txn, 'test_fiber_destroyed', $get), 'the main function has no get segment');
