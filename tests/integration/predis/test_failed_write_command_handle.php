<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT save a start time for a command whose write failed, so a
later command object that gets its handle and reads a response without being
written is not reported.

The first client points at a port where nothing listens. Its connection opens
lazily inside writeRequest(), so the write throws a connection exception after
the agent has seen the call. The command object is freed, and the next command
object takes its handle. That object is written to a working connection with
the connection's write(), which the agent does not instrument, and then read
with readResponse(). Neither command is reported.

The connection's write() exists in Predis v3 only.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../../include/config.php');
require('predis.inc');
if (version_compare(Predis\Client::VERSION, '3.0', '<')) {
  die("skip: Predis v3+ required\n");
}
*/

/*INI
*/

/*EXPECT
ok - the write failed
ok - command object reuses the handle
ok - response read
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/all
Datastore/operation/Redis/get
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_failed_write_command_handle() {
  global $REDIS_HOST, $REDIS_PORT;

  $key = randstr(16);

  $good = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
  try {
    $good->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }
  $good_connection = $good->getConnection();

  $bad = new Predis\Client(array('host' => $REDIS_HOST, 'port' => 1));
  $command = $bad->createCommand('get', array($key));
  $failed = false;
  try {
    $bad->getConnection()->writeRequest($command);
  } catch (Exception $e) {
    $failed = true;
  }
  tap_assert($failed, 'the write failed');

  /*
   * Free the exception first, since its trace can hold the command, then the
   * command, so that its handle is the next one to be reused.
   */
  unset($e);
  $handle = spl_object_id($command);
  unset($command);

  $second = $bad->createCommand('get', array($key));
  tap_equal($handle, spl_object_id($second), 'command object reuses the handle');

  $good_connection->write($second->serializeCommand());
  $response = $good_connection->readResponse($second);
  tap_equal(null, $response, 'response read');

  $good->disconnect();
}

test_failed_write_command_handle();
