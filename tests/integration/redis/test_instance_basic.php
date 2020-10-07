<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
basic operations.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - set key
ok - get key
ok - delete key
ok - delete missing key
ok - reuse deleted key
ok - set duplicate key
ok - delete key
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* the tests */
  tap_assert($redis->set($key, 'bar'), 'set key');
  tap_equal('bar', $redis->get($key), 'get key');
  tap_equal(1, $redis->del($key), 'delete key');
  tap_equal(0, $redis->del($key), 'delete missing key');

  tap_assert($redis->setnx($key, 'bar'), 'reuse deleted key');
  tap_refute($redis->setnx($key, 'bar'), 'set duplicate key');

  /* cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete key');

  /* close connection */
  $redis->close();
}

test_basic();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/get',
  'Datastore/operation/Redis/set',
  'Datastore/operation/Redis/setnx',
));

redis_datastore_instance_metric_exists($txn);
