<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster bit operations,
mirroring the coverage of tests/integration/redis/test_bitops.php.
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
ok - set initial key
ok - set second key
ok - count set bits in key
ok - find first clear bit in key
ok - find first set bit in key
ok - verify first bit clear
ok - verify second bit set
ok - clear third bit
ok - set fourth bit
ok - verify new key value
ok - perform bit operations on two keys
ok - verify value after ANDing
ok - delete test keys
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/set
Datastore/operation/Redis/get
Datastore/operation/Redis/bitcount
Datastore/operation/Redis/bitpos
Datastore/operation/Redis/getbit
Datastore/operation/Redis/setbit
Datastore/operation/Redis/bitop
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

function test_bitops_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /*
   * All three keys share a hashtag so multi-key operations
   * route to the same slot, which is required in cluster mode.
   */
  $tag  = '{rc_bitops}';
  $suffix = randstr(16);
  $key1 = $tag . '_a_' . $suffix;
  $key2 = $tag . '_b_' . $suffix;
  $dkey = $tag . '_d_' . $suffix;

  if ($cluster->exists([$key1, $key2, $dkey])) {
    die("skip: key(s) already exist: $key1, $key2, $dkey\n");
  }

  /* T - 01010100; E - 01000101; N - 01001110 */
  tap_assert($cluster->set($key1, 'TEN'), 'set initial key');
  tap_assert($cluster->set($key2, 'HAT'), 'set second key');

  tap_equal(10, $cluster->bitcount($key1), 'count set bits in key');
  tap_equal(0, $cluster->bitpos($key1, 0), 'find first clear bit in key');
  tap_equal(1, $cluster->bitpos($key1, 1), 'find first set bit in key');

  tap_equal(0, $cluster->getbit($key1, 0), 'verify first bit clear');
  tap_equal(1, $cluster->getbit($key1, 1), 'verify second bit set');

  /* T - 01010100 -> L - 01001100 */
  tap_equal(1, $cluster->setbit($key1, 3, 0), 'clear third bit');
  tap_equal(0, $cluster->setbit($key1, 4, 1), 'set fourth bit');
  tap_equal('LEN', $cluster->get($key1), 'verify new key value');

  /* |L| 01001100 |E| 01000101 |N| 01000100 &
     |H| 01001000 |A| 01000001 |T| 01010100
     |H| 01001000 |A| 01000001 |D| 01000100 */
  tap_equal(3, $cluster->bitop('AND', $dkey, $key1, $key2), 'perform bit operations on two keys');
  tap_equal('HAD', $cluster->get($dkey), 'verify value after ANDing');

  tap_equal(3, $cluster->del([$key1, $key2, $dkey]), 'delete test keys');

  $cluster->close();
}

test_bitops_cluster();
