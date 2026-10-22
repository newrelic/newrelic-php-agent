<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
The Predis instrumentation stores each command's start time keyed by the
command object's handle and never removes it. Predis v3's connection
handshake commands (HELLO, CLIENT) reach readResponse() without going through
writeRequest(). When a handshake command object reuses the handle of an
earlier command, the agent SHALL NOT record the handshake command with the
earlier command's start time.
Each iteration builds a new client, sends one GET, and frees the client with a
cycle collection. The number of other live objects varies per iteration, so
freed handles are reused in a different order each time.
*/
/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../../include/config.php');
require('predis.inc');
if (version_compare(Predis\Client::VERSION, '3.0', '<')) {
  die("skip: Predis v3+ required (connection handshake)\n");
}
*/
/*EXPECT
ok - 20 gets
*/
/*EXPECT_METRICS_EXIST
Datastore/all, 20
Datastore/operation/Redis/get, 20
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
function test_reused_command_handle() {
  global $REDIS_HOST, $REDIS_PORT;
  $key = randstr(16);
  $gets = 0;
  for ($i = 0; $i < 20; $i++) {
    /* Vary the number of other live objects: 0 to 49, fixed sequence. */
    $others = array();
    for ($j = 0; $j < ($i * 7) % 50; $j++) {
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
  tap_equal(20, $gets, '20 gets');
}
test_reused_command_handle();
