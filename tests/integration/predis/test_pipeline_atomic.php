<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The "atomic" pipeline wraps commands in a Redis EXEC call, so they happen as
an atomic operation.
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                                                            [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                                       [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                                      [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                                                 [4, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del","scope":"OtherTransaction/php__FILE__"},     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists","scope":"OtherTransaction/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pipeline"},                                       [2, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/pipeline","scope":"OtherTransaction/php__FILE__"},[2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Predis/detected"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Guzzle 4-5/detected"},                               [1, "??", "??", "??", "??", "??"]]
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
  $replies = $client->pipeline(array('atomic'), function($pipe) {
    $pipe->ping();
    $pipe->flushdb();
    $pipe->incrby($key, 7);
    $pipe->exists($key);
    $pipe->mget('does_not_exist', $key);
  });

  /* method 2 */
  $pipe = $client->pipeline(array('atomic'));
  $pipe->ping();
  $pipe->flushdb();
  $pipe->incrby($key, 13);
  $pipe->exists($key);
  $pipe->mget('does_not_exist', $key);
  $replies = $pipe->execute();

  $client->del($key);
  $client->quit();
}

test_pipeline();
