<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
After a Predis client has subscribed and unsubscribed, the commands of clients
that are created later in the same transaction SHALL be reported with their own
start times, and the commands that Predis sends on its own when a client
connects SHALL NOT be reported.

The PubSub consumer writes SUBSCRIBE and UNSUBSCRIBE with the connection's
writeRequest() and reads the replies with read(), so no readResponse() ever
runs for them. Predis v3 sends its connection handshake commands (HELLO and
CLIENT) with a single write and then calls readResponse() for each, without
writeRequest(). If a handshake command object took the handle of a freed
PubSub command, the agent would report it with that command's start time.

Each iteration subscribes and unsubscribes, frees the subscriber, allocates a
varying number of other objects so that freed handles are reused in a
different order, and then builds a new client that sends one GET. The test is
skipped below PHP 8.0, where the agent cannot tell when a command object is
freed.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../../include/config.php');
require('predis.inc');
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
*/

/*EXPECT
ok - 40 gets
*/

/*EXPECT_METRICS_EXIST
Datastore/operation/Redis/get, 40
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/hello
Datastore/operation/Redis/client
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_pubsub_then_new_client() {
  global $REDIS_HOST, $REDIS_PORT;

  $key = randstr(16);
  $gets = 0;

  /*
   * The handles of the PubSub commands are not visible from here, so the
   * test does not assert that one is reused. It runs the real flow; the
   * unit test for the command start table pins the behaviour.
   */
  for ($i = 0; $i < 40; $i++) {
    $subscriber = new Predis\Client(array('host' => $REDIS_HOST,
                                          'port' => $REDIS_PORT));
    $loop = $subscriber->pubSubLoop();
    $loop->subscribe('newrelic-test-channel');
    $loop->unsubscribe('newrelic-test-channel');
    unset($loop, $subscriber);

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
    gc_collect_cycles();
  }

  tap_equal(40, $gets, '40 gets');
}

test_pubsub_then_new_client();
