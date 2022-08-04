<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
sorted set operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
$minimum_redis_datastore_version = '5.0.0';
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - add three elements to sorted set
ok - pop maximum element off sorted set
ok - pop minimum element off sorted set
ok - we deleted our test key
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_sorted_sets() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  $key = randstr(16);
  if ($redis->exists($key)) {
    die("skip: key already exists: ${key}\n");
  }

  tap_equal(3, $redis->zadd($key, 0, 'min', 1, 'med', 2, 'max'), 'add three elements to sorted set');

  tap_equal(['max' => 2.0],  $redis->zpopmax($key), 'pop maximum element off sorted set');
  tap_equal(['min' => 0.0], $redis->zpopmin($key), 'pop minimum element off sorted set');

  tap_equal(1, $redis->del($key), 'we deleted our test key');

  $redis->close();
}

test_sorted_sets();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/del',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/zadd',
  'Datastore/operation/Redis/zpopmax',
  'Datastore/operation/Redis/zpopmin',
));

redis_datastore_instance_metric_exists($txn);
