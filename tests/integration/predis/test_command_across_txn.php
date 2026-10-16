<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT record a Predis command whose request was written in an
earlier transaction than the one in which its response is read.

The time the agent saves when a request is written is absolute. The
transaction that reads the response compares it with its own start: a request
that was written before the transaction began is not on this transaction's
timeline, and the call is not recorded, instead of being given a start and
duration measured from the wrong transaction.

Transaction 1 is ignored (newrelic_end_transaction(true)), so only the second
transaction is checked. It reads the response of the command written in
transaction 1, and then runs one command of its own: only that one is
reported.
*/
/*INI
*/
/*EXPECT
ok - response read in the next transaction
ok - second command
*/
/*EXPECT_METRICS_EXIST
Datastore/all, 1
Datastore/allOther, 1
Datastore/Redis/all, 1
Datastore/operation/Redis/get, 1
*/
/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_command_across_txn() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }

  $connection = $client->getConnection();
  $key = randstr(16);

  /* Transaction 1: the request is written, the response is not read yet. */
  $command = $client->createCommand('get', array($key));
  $connection->writeRequest($command);

  newrelic_end_transaction(true);
  newrelic_start_transaction(ini_get("newrelic.appname"));

  /* Transaction 2: the response of the request written before it began. */
  tap_equal(null, $connection->readResponse($command),
            'response read in the next transaction');
  unset($command);

  /* A command written and read inside this transaction is recorded. */
  tap_equal(null, $client->get($key), 'second command');

  $client->disconnect();
}

test_command_across_txn();
