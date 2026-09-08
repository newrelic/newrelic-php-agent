<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should report metrics for basic Memcached operations if fibers are involved: get, set, add,
increment, decrement, replace, and delete.
*/

/*SKIPIF
<?php
if (version_compare(phpversion(), '8.1', '<')) {
    die("skip: PHP >= 8.1 required\n");
}
require('skipif.inc');
?>
*/

/*INI
newrelic.fibers.disabled = false
*/

/*EXPECT
ok - add
ok - set
Starting Func 'a'
ok - replace
ok - assertEqual
ok - assertEqual
ok - delete
Ending Func 'a'
ok - delete
ok - delete
ok - add
ok - increment
ok - increment
ok - decrement
ok - decrement
ok - assertEqual
ok - delete
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "traceId": "??",
      "duration": "??",
      "transactionId": "??",
      "name": "OtherTransaction\/php__FILE__",
      "guid": "ENV[GUID_ROOT]",
      "type": "Span",
      "category": "generic",
      "priority": "??",
      "sampled": true,
      "nr.entryPoint": true,
      "timestamp": "??",
      "transaction.name": "OtherTransaction\/php__FILE__"
    },
    {},
    {}
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/main",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
    },
    {},
    {}
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Memcached\/add",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_MAIN]",
      "span.kind": "client",
      "component": "Memcached"
    },
    {},
    {
      "peer.address": "unknown:unknown"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Memcached\/delete",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_MAIN]",
      "span.kind": "client",
      "component": "Memcached"
    },
    {},
    {
      "peer.address": "unknown:unknown"
    }
  ],
  [
    {
      "category": "datastore",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Datastore\/operation\/Memcached\/decr",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_MAIN]",
      "span.kind": "client",
      "component": "Memcached"
    },
    {},
    {
      "peer.address": "unknown:unknown"
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/a",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "ENV[GUID_ROOT]"
    },
    {},
    {}
  ]
]
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/all"},                              [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Memcached/allOther"},                         [15, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/Memcached/ENV[MEMCACHE_HOST]/11211"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add"},                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/add",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/decr"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/decr",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete"},                 [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/delete",
      "scope":"OtherTransaction/php__FILE__"},                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get"},                    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/get",
      "scope":"OtherTransaction/php__FILE__"},                        [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/incr"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/incr",
      "scope":"OtherTransaction/php__FILE__"},                        [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/replace",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Memcached/set",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/get_linking_metadata"},              ["??", "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/PHP/Fiber/used"},                        ["??", "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/helpers.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/tap.php');
require_once(realpath (dirname ( __FILE__ )) . '/memcache.inc');

define('KEYLEN', 8);


function main()
{
  global $MEMCACHE_HOST, $MEMCACHE_PORT;

  env_var_for_expects("GUID_MAIN", newrelic_get_linking_metadata()['span.id'] ?? '');
  $memcached = new Memcached();
  $memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

  $key1 = randstr(KEYLEN);
  $key2 = randstr(KEYLEN);

  // basic operations
  $test = new TestCase($memcached);
  $test->add($key1, 'foo');
  $test->set($key2, 'bar');
  Fiber::suspend();
  $test->replace($key2, 'baz');
  $test->assertEqual('foo', $key1);
  $test->assertEqual('baz', $key2);
  $test->delete($key1);
  Fiber::suspend();
  $test->delete($key2);
  $test->refuteDelete($key2);

  $key3 = randstr(KEYLEN);

  // increment/decrement
  $test = new TestCase($memcached);
  $test->add($key3, 0);
  $test->increment($key3);
  $test->increment($key3, 2);
  Fiber::suspend();
  $test->decrement($key3);
  $test->decrement($key3, 2);
  $test->assertEqual(0, $key3);
  $test->delete($key3);

  $memcached->quit();
}

function a()
{
    echo "Starting Func 'a'\n";
    env_var_for_expects("GUID_A", newrelic_get_linking_metadata()['span.id'] ?? '');
    time_nanosleep(0, 100000000);
    Fiber::suspend();
    echo "Ending Func 'a'\n";
}

env_var_for_expects("GUID_ROOT", newrelic_get_linking_metadata()['span.id'] ?? '');

$fiber_a = new Fiber('a');
$fiber_memcached = new Fiber('main');

$fiber_memcached->start();
$fiber_a->start();
$fiber_memcached->resume();
$fiber_a->resume();
$fiber_memcached->resume();
$fiber_memcached->resume();
