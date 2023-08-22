<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that the Predis instrumentation still creates metrics when the transaction
is restarted mid-request.
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
    [{"name":"Datastore/operation/Redis/del","scope":"OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists"},                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/exists","scope":"OtherTransaction/php__FILE__"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/get","scope":"OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incr"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/incr","scope":"OtherTransaction/php__FILE__"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set"},                                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Datastore/operation/Redis/set","scope":"OtherTransaction/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_appname/after"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/set_appname/with_license"},                             [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},                              [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

/*EXPECT_TRACED_ERRORS null */

/*
what_DONT_EXPECT_ANALYTICS_EVENTS
null
*/

use NewRelic\Integration\Transaction;
use Predis\Client;

require_once __DIR__.'/predis.inc';
require_once(__DIR__.'/../../include/config.php');
require_once(__DIR__.'/../../include/helpers.php');
require_once(__DIR__.'/../../include/tap.php');
require_once(__DIR__.'/../../include/integration.php');

global $REDIS_HOST, $REDIS_PORT;
$client = new Predis\Client(array('host' => $REDIS_HOST, 'port' => $REDIS_PORT, 'database' => 0));

// Force predis to connect - usually it will lazily connect as needed.
// In this case the 'select' operation will still be captured because
// the connection wont occur until the 'exists' operation is executed
// below.
// Following code forces predis to connect now and therefore 'select'
// operation happens before transaction is ended below and so will not
// appear in the operations metrics
try {
  $client->connect();
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}

// Restart the transaction. If you compare the expected metrics in this test to
// test_basic.php, you'll note that the detection metrics go away (because
// they're thrown away with the initial transaction), but we still look for the
// Redis datastore metrics created by the $client method calls below.
$appname = ini_get("newrelic.appname");
$license = ini_get("newrelic.license");
newrelic_set_appname($appname, $license, false);

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
