<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Regression test for the nr_php_fcall_register_handlers registration gate, as
it applies to magic-file library detection specifically (see
observer/test_first_call_not_recording.php for the equivalent case with a
plain function call).

The Observer API invokes the fcall_init registration callback only once per
op_array per PHP request — on the file's first require/include — and caches
whatever handlers that call returns for the rest of the request. Gating the
agent's behavior in that callback on the transaction's live recording state
was wrong: a magic file required while not recording would skip
framework/library detection, so auto-instrumentation would never be
installed, even after a later transaction in the same request started
recording.

This test requires (includes) the predis/predis magic file while a transaction
is ignored, ends that transaction, then starts a fresh recording transaction
and confirms Predis calls still produce datastore metrics. That proves
nr_predis_enable() ran and installed wraprecs during the non-recording require,
independent of recording status.

Separately, the test forces an early connect and then restarts the
transaction with $xmit = false (discarding it without transmitting). That is
why the following metrics (Datastore/operation/Redis/select,
Supportability/InstrumentedFunction/Predis\Client::__construct, package
detection) are listed under EXPECT_METRICS_DONT_EXIST below — they fired
against the discarded transaction, not the one actually sent.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0 not supported\n");
}
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 1
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT
ok - key does not exist
ok - get key
*/

/*EXPECT_METRICS_EXIST
Datastore/Redis/all, 5
Datastore/operation/Redis/del, 1
Datastore/operation/Redis/exists, 1
Datastore/operation/Redis/get, 1
Datastore/operation/Redis/incr, 1
Datastore/operation/Redis/set, 1
Datastore/instance/Redis/ENV[REDIS_HOST]/6379, 5
Supportability/api/set_appname/after, 1
Supportability/api/set_appname/with_license, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/InstrumentedFunction/Predis\\Client::__construct
Datastore/operation/Redis/select
Supportability/PHP/package/predis/predis/2/detected
Supportability/library/Predis/detected
*/

/*EXPECT_TRACED_ERRORS null */

newrelic_ignore_transaction(); // turn recording off for this New Relic transaction
// zend_observer_fcall_init's callback is invoked for predis/src/client.php;
// it is the only chance to execute the library detection code and create
// predis wraprecs in nr_predis_enable(). This under normal circumstances
// would generate Supportability/library/Predis/detected which is listed
// under EXPECT_METRICS_DONT_EXIST because the transaction is not recording.
require_once realpath(getenv("PREDIS_HOME") . '/../../../').'/vendor/autoload.php';
newrelic_end_transaction(); // end New Relic transaction, which is not recording
newrelic_start_transaction(ini_get("newrelic.appname")); // Start New Relic transaction, recording is back on
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

// Restart the transaction without sending current one. If you compare the expected metrics in this test to
// test_basic.php, you'll note that the detection metrics go away (because
// they're thrown away with the initial transaction), but we still look for the
// Redis datastore metrics created by the $client method calls below.
// $xmit == false (last argument) is responsible for Datastore/operation/Redis/select,
// Supportability/InstrumentedFunction/Predis\\Client::__construct as well as
// Supportability/PHP/package/predis/predis/2/detected to be listed under
// EXPECT_METRICS_DONT_EXIST.
newrelic_set_appname(ini_get("newrelic.appname"), ini_get("newrelic.license"), false);

$key = uniqid(__FILE__, true);
tap_equal(0, $client->exists($key), 'key does not exist');

$client->set($key, 1);
$client->incr($key);
tap_equal('2', $client->get($key), 'get key');

$client->del($key);
