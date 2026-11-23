<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Each command of a Predis pipeline runs in its own asynchronous context.
*/

/*INI
newrelic.transaction_tracer.detail = 0
newrelic.transaction_tracer.threshold = 0
*/

/*EXPECT
ok - pipeline command segments found
ok - each pipeline command has its own async context
ok - the async contexts belong to the pipeline
*/

use NewRelic\Integration\Transaction;

require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/integration.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/predis.inc');

function test_pipeline_async_contexts() {
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

  $client->pipeline(function($pipe) {
    $pipe->ping();
    $pipe->exists('nr_pipeline_async_contexts');
    $pipe->mget('nr_pipeline_async_contexts_a', 'nr_pipeline_async_contexts_b');
  });

  $client->quit();
}

test_pipeline_async_contexts();

$txn = new Transaction;

$contexts = array();
foreach (array('ping', 'exists', 'mget') as $operation) {
  foreach ($txn->getTrace()->findSegmentsByName(
      'Datastore/operation/Redis/' . $operation) as $segment) {
    $contexts[] = isset($segment->attributes->async_context)
                      ? $segment->attributes->async_context
                      : null;
  }
}

tap_equal(3, count($contexts), 'pipeline command segments found');
tap_equal(3, count(array_unique($contexts)),
          'each pipeline command has its own async context');
tap_equal(3, count(preg_grep('/^Predis #/', $contexts)),
          'the async contexts belong to the pipeline');
