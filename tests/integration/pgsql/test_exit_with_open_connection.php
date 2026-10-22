<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A script that calls exit() while pgsql connections are open SHALL still have
its queries reported against the right host, and the agent SHALL shut down
cleanly. The instance saved for each connection is dropped with the
connection object during shutdown.
*/

/*SKIPIF
<?php
require("skipif.inc");
*/

/*INI
newrelic.datastore_tracer.instance_reporting.enabled = 1
newrelic.datastore_tracer.database_name_reporting.enabled = 0
*/

/*EXPECT
ok - query on first connection
ok - query on second connection
*/

/*EXPECT_METRICS_EXIST
Datastore/instance/Postgres/ENV[TEST_PG_HOST]/ENV[TEST_PG_PORT], 2
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/helpers.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');

env_var_for_expects("TEST_PG_HOST", $PG_HOST);
env_var_for_expects("TEST_PG_PORT", $PG_PORT);

$first = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
tap_assert(false !== pg_query($first, "SELECT 1"), "query on first connection");

$second = pg_connect($PG_CONNECTION, PGSQL_CONNECT_FORCE_NEW);
tap_assert(false !== pg_query($second, "SELECT 1"),
           "query on second connection");

exit(0);
