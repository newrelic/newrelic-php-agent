<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
list operations.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - append A
ok - append B
ok - append C
ok - retrieve element 0
ok - retrieve element 1
ok - retrieve element 2
ok - retrieve last element
ok - retrieve invalid element
ok - retrieve element 0
ok - retrieve element 1
ok - retrieve element 2
ok - remove first occurence of B
ok - A was not removed
ok - C was not removed
ok - B was removed
ok - remove missing element
ok - replace list head
ok - list head was replaced
ok - delete list
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_redis() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* Ensure the key doesn't persist (too much) longer than the test. */
  $redis->expire($key, 30 /* seconds */);

  tap_equal(1, $redis->rPush($key, 'A'), 'append A');
  tap_equal(2, $redis->rPush($key, 'B'), 'append B');
  tap_equal(3, $redis->rPush($key, 'C'), 'append C');

  tap_equal('A', $redis->lGet($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lGet($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lGet($key, 2), 'retrieve element 2');
  tap_equal('C', $redis->lGet($key, -1), 'retrieve last element');
  tap_refute($redis->lGet($key, 10), 'retrieve invalid element');

  tap_equal('A', $redis->lIndex($key, 0), 'retrieve element 0');
  tap_equal('B', $redis->lIndex($key, 1), 'retrieve element 1');
  tap_equal('C', $redis->lIndex($key, 2), 'retrieve element 2');

  tap_equal(1, $redis->lRemove($key, 'B', 1), 'remove first occurence of B');
  tap_equal('A', $redis->lGet($key, 0), 'A was not removed');
  tap_equal('C', $redis->lGet($key, 1), 'C was not removed');
  tap_refute($redis->lGet($key, 2), 'B was removed');
  tap_equal(0, $redis->lRem($key, 'B', 1), 'remove missing element');

  tap_assert($redis->lSet($key, 0, 'AA'), 'replace list head');
  tap_equal('AA', $redis->lGet($key, 0), 'list head was replaced');

  /* cleanup the key used by this test run */
  tap_equal(1, $redis->del($key), 'delete list');

  $redis->close();
}

test_redis();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/lget',
  'Datastore/operation/Redis/lindex',
  'Datastore/operation/Redis/lrem',
  'Datastore/operation/Redis/lremove',
  'Datastore/operation/Redis/lset',
));

redis_datastore_instance_metric_exists($txn);
