<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

/* Maps Redis commands to their minimum version. */
$REDIS_FEATURES = array(
  'HINCRBYFLOAT' => '2.6.0',
  'INCRBYFLOAT'  => '2.6.0'
);

/* Determine whether a Redis instance supports the given command. */
function redis_supports($redis, $command)
{
  global $REDIS_FEATURES;

  if (isset($REDIS_FEATURES[$command])) {
    $info = $redis->info();
    $version = $info['redis_version'];
    $min_version = $REDIS_FEATURES[$command];
    return version_compare($version, $min_version, '>=');
  }

  return TRUE;
}

function redis_datastore_instance_metric_exists(Transaction $txn)
{
  global $REDIS_HOST, $REDIS_PORT;

  $metrics = $txn->getUnscopedMetrics();
  $host = newrelic_is_localhost($REDIS_HOST) ? newrelic_get_hostname() : $REDIS_HOST;
  $port = (string) $REDIS_PORT;
  tap_assert(isset($metrics["Datastore/instance/Redis/$host/$port"]), 'datastore instance metric exists');
}

function redis_trace_nodes_match(Transaction $txn, array $operations)
{
  global $REDIS_HOST, $REDIS_PORT;

  $ok = true;
  $trace = $txn->getTrace();
  $nodes = iterator_to_array($trace->findSegmentsWithDatastoreInstances());

  /*
   *  array_flip() gives us an array with the expected operations as keys
   * (effectively a string set), which means we can do simple hashmap lookups
   * for each operation rather than walking the array each time.
  */
  $expected = array_flip($operations);

  /*
   * Ensure that there are no unexpected operation types, and that whatever
   * nodes exist have instance information. We can't do more than that because
   * extremely fast Redis operations may not generate trace nodes, which then
   * leads to test instability.
   *
   * Since we don't know how many nodes we're going to get, we can't use
   * tap_assert(), since it will generate a variable number of assertion
   * messages and the test runner isn't smart enough to figure that out.
   * Instead, if something fails, we'll use tap_not_ok() to generate an
   * unexpected failure message with (hopefully) useful state.
   */
  foreach ($nodes as $i => $node) {
    if (!array_key_exists($node->name, $expected)) {
      tap_not_ok("trace node $i operation is not in the expected list", implode('; ', $operations), $node->name);
      $ok = false;
    }

    $instance = $node->getDatastoreInstance();

    if (!$instance->isHost($REDIS_HOST)) {
      tap_not_ok("trace node $i host does not match", $REDIS_HOST, $instance->host);
      $ok = false;
    }

    if ($REDIS_PORT != $instance->portPathOrId) {
      tap_not_ok("trace node $i port does not match", $REDIS_PORT, $instance->portPathOrId);
      $ok = false;
    }

    if ('0' !== $instance->databaseName) {
      tap_not_ok("trace node $i database does not match", '0', $instance->databaseName);
      $ok = false;
    }
  }

  if ($ok) {
    tap_ok('trace nodes match');
  }
}
