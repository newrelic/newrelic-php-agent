<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Predis provides command pipelines. In these cases, we don't know what commands
are being run, so they are instrumented as "pipeline".
*/

/*INI
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/all"},                                      [13, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                 [13, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                [13, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                           [13, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/instance/Redis/__HOST__/6379"},             [13, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                   [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/flushdb"},                  [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/flushdb",
      "scope":"OtherTransaction/php__FILE__"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incrby"},                   [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incrby",
      "scope":"OtherTransaction/php__FILE__"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget"},                     [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/mget",
      "scope":"OtherTransaction/php__FILE__"},                      [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ping"},                     [3, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/ping",
      "scope":"OtherTransaction/php__FILE__"},                      [3, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Predis/detected"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 4-5/detected"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


/*EXPECT_TRACED_ERRORS null */

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_pipeline() {
  global $REDIS_HOST, $REDIS_PORT;
  $client = new Predis\Client(array(
    'host' => $REDIS_HOST,
    'port' => $REDIS_PORT,
  ));

  try {
      $client->connect();
  } catch (Exception $e) {
      die("skip: " . $e->getMessage() . "\n");
  }

  /* generate a unique key to use for this test run */
  $key = randstr(16);
  if ($client->exists($key)) {
    echo "key already exists: ${key}\n";
    exit(1);
  }

  /* method 1 */
  $replies = $client->pipeline(function($pipe) {
    global $key;
    $pipe->ping();
    $pipe->flushdb();
    $pipe->incrby($key, 7);
    $pipe->exists($key);
    $pipe->mget('does_not_exist', $key);
  });

  /* method 2 */
  $pipe = $client->pipeline();
  $pipe->ping();
  $pipe->flushdb();
  $pipe->incrby($key, 13);
  $pipe->exists($key);
  $pipe->mget('does_not_exist', $key);
  $replies = $pipe->execute();

  /* method 3 (exception) */
  $replies = $client->pipeline(function($pipe) {
    try {
      $pipe->execute();
    } catch (Exception $e) {
      $pipe->ping();
    }
  });

  $client->del($key);
  $client->quit();
}

test_pipeline();
