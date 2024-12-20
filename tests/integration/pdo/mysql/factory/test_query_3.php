<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record database metrics for the FETCH_CLASS variant of
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
Datastore/MySQL/all, 4
Datastore/MySQL/allOther, 4
Datastore/operation/MySQL/create, 1
Datastore/statement/MySQL/test/create, 1
Datastore/operation/MySQL/insert, 1
Datastore/statement/MySQL/test/insert, 1
Datastore/operation/MySQL/select, 1
Datastore/statement/MySQL/test/select, 1
Datastore/operation/MySQL/drop, 1
Datastore/statement/MySQL/test/drop, 1
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../test_query_3.inc');
require_once(realpath (dirname ( __FILE__ )) . '/../../../../include/config.php');

test_pdo_query(PDO::connect($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD), 0);
