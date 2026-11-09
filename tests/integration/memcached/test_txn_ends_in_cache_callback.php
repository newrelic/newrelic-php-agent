<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that the cache callback of Memcached::get() ends and starts again
SHALL NOT break the agent, and the call SHALL NOT be recorded in either
transaction.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Memcached/get
*/

/*EXPECT_TRACED_ERRORS
null
*/

// The use of freed memory that this test guards against is only detected
// under valgrind.
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$memcached = new Memcached();
$memcached->addServer($MEMCACHE_HOST, $MEMCACHE_PORT);

$memcached->get('nr_txn_ends_key_that_does_not_exist',
                function ($memcached, $key, &$value) {
                  newrelic_end_transaction();
                  newrelic_start_transaction(ini_get("newrelic.appname"));
                  echo "transaction restarted\n";
                  return false;
                });
echo "done\n";
