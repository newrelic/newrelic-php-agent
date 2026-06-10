<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL report Datastore metrics for Redis basic operations when
using Predis in cluster mode.
*/

/*SKIPIF
<?php
require('predis.inc');
*/

/*EXPECT
ok - set key
ok - get key
ok - delete key
ok - delete missing key
ok - reuse deleted key
ok - set duplicate key
ok - delete key
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/instance/Redis/redisclusterdb/7000
Datastore/operation/Redis/del
Datastore/operation/Redis/exists
Datastore/operation/Redis/get
Datastore/operation/Redis/set
Datastore/operation/Redis/setnx
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_cluster_basic() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $client = new Predis\Client(
    array(
      array('host' => $REDIS_CLUSTER_HOST, 'port' => $REDIS_CLUSTER_PORT),
    ),
    array('cluster' => 'redis')
  );

  try {
    $client->connect();
  } catch (Exception $e) {
    die("skip: " . $e->getMessage() . "\n");
  }

  /* Generate a unique key to use for this test run */
  $key = randstr(16);
  if ($client->exists($key)) {
    echo "key already exists: $key\n";
    exit(1);
  }

  $rval = $client->set($key, 'bar');
  tap_equal('OK', $rval->getPayload(), 'set key');
  tap_equal('bar', $client->get($key), 'get key');
  tap_equal(1, $client->del($key), 'delete key');
  tap_equal(0, $client->del($key), 'delete missing key');

  tap_assert($client->setnx($key, 'bar') == 1, 'reuse deleted key');
  tap_refute($client->setnx($key, 'bar') == 1, 'set duplicate key');

  /* Cleanup the key used by this test run */
  tap_equal(1, $client->del($key), 'delete key');

  /* Close connection */
  $client->disconnect();
}

test_cluster_basic();
