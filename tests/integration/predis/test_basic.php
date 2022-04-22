<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL report Datastore metrics for Redis basic operations.
This Predis test is largely copied from the Redis version.
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

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                                           [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                                      [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                                     [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                                                [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/Redis/__HOST__/6379"},                                  [8, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                                           [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del","scope":"OtherTransaction/php__FILE__"},    [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists","scope":"OtherTransaction/php__FILE__"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get","scope":"OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set","scope":"OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx"},                                         [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/setnx","scope":"OtherTransaction/php__FILE__"},  [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Predis/detected"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 4-5/detected"},                              [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_basic() {
  global $REDIS_HOST, $REDIS_PORT;
  $client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));

  try {
      $client->connect();
  } catch (Exception $e) {
      die("skip: " . $e->getMessage() . "\n");
  }

  /* Generate a unique key to use for this test run */
  $key = randstr(16);
  if ($client->exists($key)) {
      echo "key already exists: ${key}\n";
      exit(1);
  }

  /* The tests */
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

test_basic();
