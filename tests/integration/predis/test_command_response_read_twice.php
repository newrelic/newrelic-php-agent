<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL report a Predis command once, even if readResponse() is called
a second time with the same command object.

The agent stores a start time in writeRequest() and uses it, once, in the
matching readResponse(). A second readResponse() with the same command object
has no matching write, so it has no start time and is not reported. Before the
start time was removed after the first read, the second read was reported again
with the first command's start time.

The second response is produced by sending the command's bytes straight to the
socket, so there is a response to read without a writeRequest().
*/

/*INI
*/

/*EXPECT
ok - first response read
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

function test_command_response_read_twice() {
  global $REDIS_HOST, $REDIS_PORT;

  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }

  $connection = $client->getConnection();
  $key = randstr(16);

  $command = $client->createCommand('get', array($key));
  $connection->writeRequest($command);
  tap_equal(null, $connection->readResponse($command), 'first response read');

  /* A second response for the same command object, with no writeRequest(). */
  write_raw($connection,
            "*2\r\n\$3\r\nGET\r\n\$" . strlen($key) . "\r\n" . $key . "\r\n");
  tap_equal(null, $connection->readResponse($command), 'second response read');

  $client->disconnect();
}

test_command_response_read_twice();
