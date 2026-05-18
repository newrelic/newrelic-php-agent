<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster setex and
expiry operations, mirroring the coverage of tests/integration/redis/test_setex.php.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT
ok - set key to expire in 1s
ok - retrieve key before expiry
ok - check key ttl
ok - retrieve expired key
ok - delete expired key
ok - set key to expire in 1500ms
ok - retreive key before expiry
ok - retreive key ttl in miliseconds
ok - remove key expiration
ok - verify expiry removal
ok - set timestamp expiry
ok - verify expiry set
ok - set timestamp expiry in milliseconds
ok - verify milliscond expiry set
ok - set an expiry in milliseconds
ok - verify millisecond expiry
ok - delete key
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/setex
Datastore/operation/Redis/get
Datastore/operation/Redis/ttl
Datastore/operation/Redis/psetex
Datastore/operation/Redis/pttl
Datastore/operation/Redis/persist
Datastore/operation/Redis/expireat
Datastore/operation/Redis/pexpireat
Datastore/operation/Redis/pexpire
Datastore/operation/Redis/del
Datastore/operation/Redis/exists
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/connect
Datastore/operation/Redis/pconnect
Datastore/operation/Redis/open
Datastore/operation/Redis/popen
Datastore/operation/Redis/select
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

function test_setex_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($cluster->exists($key)) {
    die("skip: key already exists: $key\n");
  }

  tap_assert($cluster->setex($key, 1, 'bar'), 'set key to expire in 1s');
  tap_equal('bar', $cluster->get($key), 'retrieve key before expiry');
  tap_assert($cluster->ttl($key) == 1, 'check key ttl');
  sleep(2);
  tap_refute($cluster->get($key), 'retrieve expired key');
  tap_equal(0, $cluster->del($key), 'delete expired key');  // it is no longer there: auto expiration

  tap_assert($cluster->psetex($key, 1500, 'pbar'), 'set key to expire in 1500ms');
  tap_equal('pbar', $cluster->get($key), 'retreive key before expiry');

  $pttl = $cluster->pttl($key);
  tap_assert($pttl >= 0 && $pttl <= 1500, 'retreive key ttl in miliseconds');

  tap_assert($cluster->persist($key), 'remove key expiration');
  tap_equal(-1, $cluster->ttl($key), 'verify expiry removal');

  tap_assert($cluster->expireat($key, time() + 1), 'set timestamp expiry');
  tap_assert($cluster->ttl($key) >= 0, 'verify expiry set');

  tap_assert($cluster->pexpireat($key, (time() + 60) * 1000), 'set timestamp expiry in milliseconds');
  tap_assert($cluster->ttl($key) > 50, 'verify milliscond expiry set');

  tap_assert($cluster->pexpire($key, 1500), 'set an expiry in milliseconds');
  tap_assert($cluster->pttl($key) >= 1000, 'verify millisecond expiry');

  tap_equal(1, $cluster->del($key), 'delete key');

  $cluster->close();
}

test_setex_cluster();
