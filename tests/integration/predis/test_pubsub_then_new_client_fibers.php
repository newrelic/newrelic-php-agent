<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
After a Predis client has subscribed and unsubscribed in one fiber, the
commands of clients that other fibers create SHALL be reported with their own
start times, and the commands that Predis sends on its own when a client
connects SHALL NOT be reported.

One fiber subscribes and unsubscribes with a new client on every iteration, and
frees the client. Another fiber builds a new client and sends one GET on every
iteration, after a varying number of other objects has been allocated. The
fibers take turns, so handles that one fiber frees are reused by objects of the
other.
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
ok - 30 gets
ok - the subscribing fiber has no get segment
ok - no hello segment
ok - no client segment
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Redis/get, 30
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/hello
Datastore/operation/Redis/client
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

function subscribing_fiber() {
  global $REDIS_HOST, $REDIS_PORT;

  for ($i = 0; $i < 30; $i++) {
    $subscriber = new Predis\Client(array('host' => $REDIS_HOST,
                                          'port' => $REDIS_PORT));
    $loop = $subscriber->pubSubLoop();
    $loop->subscribe('newrelic-test-channel');
    $loop->unsubscribe('newrelic-test-channel');
    unset($loop, $subscriber);
    Fiber::suspend();
  }
}

function getting_fiber() {
  global $REDIS_HOST, $REDIS_PORT;

  $key = randstr(16);
  $gets = 0;

  for ($i = 0; $i < 30; $i++) {
    /* Vary the number of other live objects: 0 to 10, fixed sequence. */
    $others = array();
    for ($j = 0; $j < ($i * 3) % 11; $j++) {
      $others[] = new stdClass();
    }

    $client = new Predis\Client(array('host' => $REDIS_HOST,
                                      'port' => $REDIS_PORT));
    if (null === $client->get($key)) {
      $gets++;
    }
    unset($client, $others);
    Fiber::suspend();
  }

  tap_equal(30, $gets, '30 gets');
}

$subscribing = new Fiber('subscribing_fiber');
$getting = new Fiber('getting_fiber');

$subscribing->start();
$getting->start();
while (!$subscribing->isTerminated() || !$getting->isTerminated()) {
  if (!$subscribing->isTerminated()) {
    $subscribing->resume();
  }
  if (!$getting->isTerminated()) {
    $getting->resume();
  }
}

$txn = new Transaction;
$get = 'Datastore/operation/Redis/get';
tap_equal(0, count_children($txn, 'subscribing_fiber', $get), 'the subscribing fiber has no get segment');
tap_equal(0, iterator_count($txn->getTrace()->findSegmentsByName('Datastore/operation/Redis/hello')), 'no hello segment');
tap_equal(0, iterator_count($txn->getTrace()->findSegmentsByName('Datastore/operation/Redis/client')), 'no client segment');
