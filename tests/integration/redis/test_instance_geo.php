<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report Redis metrics including instance information for Redis
basic operations.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '5.4', '<')) {
    die("skip: PHP > 5.3 required\n");
}
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
*/

/*EXPECT
ok - add Seattle to a geo sorted set
ok - add Cupertino to geo sorted set
ok - add Tokyo to geo sorted set
ok - query distance between two cities
ok - query Tokyo geohash string
ok - verify Seattle geopos in redis
ok - query cities within 1000mi of Seattle
ok - query cities within 1000mi of Seattle [readonly cmd]
ok - query cities within 5000mi of seattle by member
ok - query cities within 5000mi of seattle by member [readonly cmd]
ok - trace nodes match
ok - datastore instance metric exists
*/

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/integration.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/redis.inc');

function test_geo() {
  global $REDIS_HOST, $REDIS_PORT;

  $redis = new Redis();
  $redis->connect($REDIS_HOST, $REDIS_PORT);

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($redis->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  $seattle = [-122.335167, 47.608013];
  $cupertino = [-122.032182, 37.322998];
  $tokyo = [139.839478, 35.652832];

  tap_equal(1, $redis->geoadd($key, $seattle[0], $seattle[1], 'Seattle'), 'add Seattle to a geo sorted set');
  tap_equal(1, $redis->geoadd($key, $cupertino[0], $cupertino[1], 'Cupertino'), 'add Cupertino to geo sorted set');
  tap_equal(1, $redis->geoadd($key, $tokyo[0], $tokyo[1], 'Tokyo'), 'add Tokyo to geo sorted set');

  /* Values calculated in another redis-cli session */
  tap_equal(711.0, round($redis->geodist($key, 'Seattle', 'Cupertino', 'mi')), 'query distance between two cities');
  tap_equal(["xn76y4khb10"], $redis->geohash($key, "Tokyo"), 'query Tokyo geohash string');

  list($redis_lat, $redis_lng) = $redis->geopos($key, 'Seattle')[0];
  tap_equal([round($seattle[0], 2), round($seattle[1], 2)], [round($redis_lat, 2), round($redis_lng, 2)],
            'verify Seattle geopos in redis');

  list ($lat, $long) = $seattle;
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $redis->georadius($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle');
  tap_equal_unordered_values(['Seattle', 'Cupertino'], $redis->georadius_ro($key, $lat, $long, 1000, 'mi'), 'query cities within 1000mi of Seattle [readonly cmd]');

  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $redis->georadiusbymember($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member');
  tap_equal_unordered_values(['Seattle', 'Cupertino', 'Tokyo'], $redis->georadiusbymember_ro($key, 'Seattle', 5000, 'mi'), 'query cities within 5000mi of seattle by member [readonly cmd]');

  /* close connection */
  $redis->close();
}

test_geo();

$txn = new Transaction;

redis_trace_nodes_match($txn, array(
  'Datastore/operation/Redis/close',
  'Datastore/operation/Redis/connect',
  'Datastore/operation/Redis/exists',
  'Datastore/operation/Redis/geoadd',
  'Datastore/operation/Redis/geodist',
  'Datastore/operation/Redis/geohash',
  'Datastore/operation/Redis/geopos',
  'Datastore/operation/Redis/georadius',
  'Datastore/operation/Redis/georadius_ro',
  'Datastore/operation/Redis/georadiusbymember',
  'Datastore/operation/Redis/georadiusbymember_ro',
));

redis_datastore_instance_metric_exists($txn);
