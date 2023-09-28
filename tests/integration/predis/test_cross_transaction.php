<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent SHALL report Datastore metrics for Redis basic operations, even if the
client initialization happens in a different transaction than the connection.
*/

/*INI
*/

/*EXPECT
ok - set key
ok - get key
ok - delete key
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                        [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                   [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                  [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                             [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/Redis/redisdb/6379"},                [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set",
      "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_appname/after"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
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

  newrelic_set_appname(ini_get("newrelic.appname"));

  try {
      $client->connect();
  } catch (Exception $e) {
      die("skip: " . $e->getMessage() . "\n");
  }

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($client->exists($key)) {
      echo "key already exists: $key\n";
      exit(1);
  }

  /* the tests */
  $rval = $client->set($key, 'bar');
  tap_equal('OK', $rval->getPayload(), 'set key');
  tap_equal('bar', $client->get($key), 'get key');

  /* cleanup the key used by this test run */
  tap_equal(1, $client->del($key), 'delete key');

  /* close connection */
  $client->disconnect();
}

test_basic();
