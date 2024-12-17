<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record Datastore metrics for the one argument form of
PDO::query() when PDO::connect factory method is used to create connection
object.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_mysql.inc');
require(realpath (dirname ( __FILE__ )) . '/../../skipif_pdo_subclasses.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_TRACED_ERRORS null*/

/*EXPECT_METRICS_EXIST
Datastore/Postgres/all
*/

/*EXPECT
ok - create table
ok - insert one
ok - insert two
ok - insert three
ok - query (1-arg)
ok - drop table
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 6
Datastore/allOther, 6
Datastore/Postgres/all, 6
Datastore/Postgres/allOther, 6
Datastore/operation/Postgres/create, 1
Datastore/operation/Postgres/drop, 1
Datastore/operation/Postgres/insert, 3
Datastore/operation/Postgres/select, 1
Datastore/statement/Postgres/test/create, 1
Datastore/statement/Postgres/test/drop, 1
Datastore/statement/Postgres/test/insert, 3
Datastore/statement/Postgres/test/select, 1
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_query_1.inc');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

$conn = PDO::connect($PDO_PGSQL_DSN, $PG_USER, $PG_PW);
test_pdo_query($conn, 0);
