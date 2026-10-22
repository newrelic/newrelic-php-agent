<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT record a Predis command that it did not see being written,
even when an earlier command with the same object handle was read in a previous
transaction.

The start time the agent stores for a command is an offset from the start of
the transaction in which it was stored. The map lives for the whole PHP request,
so under FrankenPHP worker mode entries written by earlier requests are still
there. If the entry is not removed after the read, a later command object that
reuses the handle and reaches readResponse() without writeRequest() is recorded
in the new transaction with a start offset from the old one.

Transaction 1 is ignored (newrelic_end_transaction(true)), so only the second
transaction is checked: it contains one real command and one that was not
written through the connection, and only the real one is reported.
*/

/*INI
*/

/*EXPECT
ok - first response read
ok - command object reuses the handle
ok - second response read
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

/*
 * Predis v3 returns a stream wrapper object from getResource(), earlier
 * versions a PHP stream resource.
 */
function write_raw($connection, $bytes) {
  $resource = $connection->getResource();
  if (is_object($resource)) {
    $resource->write($bytes);
  } else {
    fwrite($resource, $bytes);
  }
}

function test_reused_command_handle_no_write_txn_restart() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }

  $connection = $client->getConnection();
  $key = randstr(16);

  /* Transaction 1: a command that is written and read, then freed. */
  $command = $client->createCommand('get', array($key));
  $connection->writeRequest($command);
  $connection->readResponse($command);
  $handle = spl_object_id($command);
  unset($command);

  newrelic_end_transaction(true);
  newrelic_start_transaction(ini_get("newrelic.appname"));

  /* Transaction 2: a command that is written and read as usual. */
  $real = $client->createCommand('get', array($key));
  $connection->writeRequest($real);
  tap_equal(null, $connection->readResponse($real), 'first response read');
  unset($real);

  /*
   * A command object that takes a freed handle and reaches readResponse()
   * without writeRequest(), because its bytes go straight to the socket.
   */
  $second = $client->createCommand('get', array($key));
  tap_equal($handle, spl_object_id($second), 'command object reuses the handle');

  write_raw($connection,
            "*2\r\n\$3\r\nGET\r\n\$" . strlen($key) . "\r\n" . $key . "\r\n");
  tap_equal(null, $connection->readResponse($second), 'second response read');

  $client->disconnect();
}

test_reused_command_handle_no_write_txn_restart();
