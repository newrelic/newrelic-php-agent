<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Datastore/Redis metrics for \RedisCluster increment
operations, mirroring the coverage of tests/integration/redis/test_incr.php.
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
ok - set key
ok - check value
ok - increment by 1
ok - check value
ok - increment by 3
ok - check final value
ok - delete key
*/

/*EXPECT_METRICS_EXIST
Datastore/all
Datastore/allOther
Datastore/Redis/all
Datastore/Redis/allOther
Datastore/operation/Redis/set
Datastore/operation/Redis/get
Datastore/operation/Redis/incr
Datastore/operation/Redis/incrby
Datastore/operation/Redis/del
Datastore/operation/Redis/exists
Datastore/operation/Redis/expire
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

function test_incr_cluster() {
  global $REDIS_CLUSTER_HOST, $REDIS_CLUSTER_PORT;

  $cluster = new RedisCluster(null,
    [$REDIS_CLUSTER_HOST . ':' . $REDIS_CLUSTER_PORT],
    1.5, 1.5, true);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($cluster->exists($key)) {
    die("skip: key already exists: $key\n");
  }

  /* Ensure the key doesn't persist (too much) longer than the test. */
  $cluster->expire($key, 30 /* seconds */);

  tap_assert($cluster->set($key, 20), 'set key');
  tap_equal('20', $cluster->get($key), 'check value');
  tap_equal(21, $cluster->incr($key), 'increment by 1');
  tap_equal('21', $cluster->get($key), 'check value');
  tap_equal(24, $cluster->incrBy($key, 3), 'increment by 3');
  tap_equal('24', $cluster->get($key), 'check final value');

  /* cleanup the key used by this test run */
  tap_equal(1, $cluster->del($key), 'delete key');

  $cluster->close();
}

test_incr_cluster();
