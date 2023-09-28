<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test basic Predis functionality with datastore instance reporting disabled.
*/

/*SKIPIF
<?php
if (!function_exists('newrelic_get_trace_json')) {
  die("skip: release builds of the agent do not include newrelic_get_trace_json()");
}
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT
ok - key does not exist
ok - get key
ok - no instance metadata found
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"Datastore/all"},                                                           [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/allOther"},                                                      [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/all"},                                                     [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/Redis/allOther"},                                                [5, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/del",
      "scope":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists",
      "scope":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get",
      "scope":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incr"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incr",
      "scope":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set",
      "scope":"OtherTransaction/php__FILE__"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/library/Predis/detected"},                                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},                     [1, "??", "??", "??", "??", "??"]]
  ]
]
*/


/*EXPECT_TRACED_ERRORS null */

use NewRelic\Integration\Transaction;
use Predis\Client;

require_once __DIR__.'/predis.inc';
require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/../../include/integration.php');

global $REDIS_HOST, $REDIS_PORT;
$client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT));
try {
  $client->connect();
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}

$key = uniqid(__FILE__, true);
tap_equal(0, $client->exists($key), 'key does not exist');

$client->set($key, 1);
$client->incr($key);
tap_equal('2', $client->get($key), 'get key');

$client->del($key);

// Test that we did not generate datastore instance metadata.
$txn = new Transaction;
foreach ($txn->getTrace()->findSegmentsByName('Datastore/operation/Redis/set') as $segment) {
  tap_equal(null, $segment->getDatastoreInstance(), 'no instance metadata found');
  break;
}
