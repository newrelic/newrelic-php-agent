<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record database metrics for the FETCH_CLASS variant of
PDO::query() when PDO base class constructor is used to create connection
object.
*/

/*SKIPIF
<?php require(realpath (dirname ( __FILE__ )) . '/../../skipif_pgsql.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
*/

/*EXPECT_ERROR_EVENTS null*/

/*EXPECT
ok - create table
ok - insert row
ok - fetch row as object
ok - drop table
*/

/*EXPECT_METRICS_EXIST
Datastore/all, 4
Datastore/allOther, 4
Datastore/Postgres/all, 4
Datastore/Postgres/allOther, 4
Datastore/operation/Postgres/create, 1
Datastore/statement/Postgres/test/create, 1
Datastore/operation/Postgres/insert, 1
Datastore/statement/Postgres/test/insert, 1
Datastore/operation/Postgres/select, 1
Datastore/statement/Postgres/test/select, 1
Datastore/operation/Postgres/drop, 1
Datastore/statement/Postgres/test/drop, 1
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_query_3.inc');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

test_pdo_query(new PDO($PDO_PGSQL_DSN, $PG_USER, $PG_PW), 0);
