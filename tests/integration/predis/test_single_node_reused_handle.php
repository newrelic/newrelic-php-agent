<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A client with a single node connection saves the node's datastore instance
when the client is built. When a later client's node connection to a different
host reuses a freed node connection's handle, the agent SHALL report its
commands against the later node's host.

The two nodes are two names for the same server: REDIS_HOST and its IP address.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/../../include/config.php');
require('predis.inc');

$ip = gethostbyname($REDIS_HOST);
if ($ip === $REDIS_HOST) {
  die("skip: REDIS_HOST must be a host name that resolves to an IP address\n");
}

// The agent reports all loopback names as the system hostname.
foreach (array($REDIS_HOST, $ip) as $h) {
  if (in_array($h, array("localhost", "127.0.0.1", "0.0.0.0", "::1"))) {
    die("skip: REDIS_HOST must not be a loopback address\n");
  }
}
*/

/*EXPECT
ok - set key via first client (host name)
ok - later node connection reuses the first node connection's handle
ok - get key via later client (IP)
ok - delete key via later client (IP)
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Redis/ENV[TEST_REDIS_NAME]/ENV[TEST_REDIS_PORT]
Datastore/instance/Redis/ENV[TEST_REDIS_IP]/ENV[TEST_REDIS_PORT]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function new_client($host, $port) {
  return new Predis\Client(array('host' => $host, 'port' => $port));
}

/*
 * getConnectionByKey() isn't wrapped by the agent, so reading the node
 * connection this way doesn't save anything.
 */
function node_handle($client, $key) {
  return spl_object_id($client->getConnection());
}

function test_single_node_reused_handle() {
  global $REDIS_HOST, $REDIS_PORT;

  $name = $REDIS_HOST;
  $ip = gethostbyname($REDIS_HOST);
  env_var_for_expects("TEST_REDIS_NAME", $name);
  env_var_for_expects("TEST_REDIS_IP", $ip);
  env_var_for_expects("TEST_REDIS_PORT", $REDIS_PORT);

  $key = randstr(16);

  /* First client: the SET routes through the aggregate, saved as the name. */
  $client = new_client($name, $REDIS_PORT);
  $first_handle = node_handle($client, $key);
  $rval = $client->set($key, 'bar');
  tap_equal('OK', $rval->getPayload(), 'set key via first client (host name)');

  /* Free the client; its node connection goes only when cycles are collected. */
  unset($client, $rval);
  gc_collect_cycles();

  /*
   * Build clients on the IP until one's node connection takes the freed
   * handle. Unused clients send no commands, so the agent saves nothing for
   * them.
   */
  $handle = null;
  for ($i = 0; $i < 20; $i++) {
    $client = new_client($ip, $REDIS_PORT);
    $handle = node_handle($client, $key);
    if ($handle === $first_handle) {
      break;
    }
    unset($client);
    gc_collect_cycles();
  }
  tap_equal($first_handle, $handle,
            "later node connection reuses the first node connection's handle");

  /* Both commands must be reported against the IP. */
  tap_equal('bar', $client->get($key), 'get key via later client (IP)');
  tap_equal(1, $client->del($key), 'delete key via later client (IP)');
}

test_single_node_reused_handle();
