<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT record a Predis command that it did not see being written.

The agent stores a start time for each command when the connection's
writeRequest() is called, keyed by the command object's handle, and uses it when
the matching readResponse() is called. PHP reuses the handle of a freed object,
so if the entry is not removed after the read, a later command object that gets
the same handle and reaches readResponse() without going through writeRequest()
(as the handshake commands of Predis v3 do) is recorded with the earlier
command's start time.

This test sends a second command's bytes straight to the socket, so it reaches
readResponse() with no writeRequest(), and gives it the handle of a command
that has already been read. Only the first command is reported.
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

function test_reused_command_handle_no_write() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }

  $connection = $client->getConnection();
  $key = randstr(16);

  /*
   * A normal command: written and read through the connection, so the agent
   * stores a start time under its handle and uses it for the response.
   */
  $command = $client->createCommand('get', array($key));
  $connection->writeRequest($command);
  $response = $connection->readResponse($command);
  tap_equal(null, $response, 'first response read');

  $handle = spl_object_id($command);
  unset($command);

  /*
   * The next command object takes the freed handle. Its request is written
   * directly to the socket, so the connection's writeRequest() never runs for
   * it, and only readResponse() does.
   */
  $second = $client->createCommand('get', array($key));
  tap_equal($handle, spl_object_id($second), 'command object reuses the handle');

  write_raw($connection,
            "*2\r\n\$3\r\nGET\r\n\$" . strlen($key) . "\r\n" . $key . "\r\n");
  $response = $connection->readResponse($second);
  tap_equal(null, $response, 'second response read');

  $client->disconnect();
}

test_reused_command_handle_no_write();
