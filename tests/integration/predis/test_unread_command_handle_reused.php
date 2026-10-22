<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL NOT record a Predis command that it did not see being written,
even when an earlier command with the same handle was written in the same
transaction and its response was never read.

The agent stores a start time for each command when the connection's
writeRequest() is called, and removes it when readResponse() is called for that
command. If the response is never read, the entry would stay. PHP reuses the
handle of a freed object, so a later command object that gets the same handle
and reaches readResponse() without going through writeRequest() (as the
handshake commands of Predis v3 do) would be recorded with the earlier
command's start time.

This test writes a command and frees it without reading its response. The next
command object takes its handle and reads the pending response with no
writeRequest() of its own. Neither command is reported.

The test is skipped below PHP 8.0, where the agent cannot tell when a command
object is freed.
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

function test_unread_command_handle_reused() {
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
   * Written through the connection, so the agent stores a start time under
   * its handle, but its response is never read through this object.
   */
  $command = $client->createCommand('get', array($key));
  $connection->writeRequest($command);

  $handle = spl_object_id($command);
  unset($command);

  /*
   * The next command object takes the freed handle and reads the pending
   * response; the connection's writeRequest() never runs for it.
   */
  $second = $client->createCommand('get', array($key));
  tap_equal($handle, spl_object_id($second), 'command object reuses the handle');

  $response = $connection->readResponse($second);
  tap_equal(null, $response, 'response read');

  $client->disconnect();
}

test_unread_command_handle_reused();
