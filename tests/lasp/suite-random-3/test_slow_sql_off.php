<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that Language Agent Security Policy (LASP) has not introduced any
bugs around the behavior of newrelic.transaction_tracer.record_sql = "off".
(i.e. transaction is not eligible for slow sql traces and we send nothing)
*/

/*SKIPIF
<?php require('../../integration/pdo/skipif_mysql.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "off"
*/

/*EXPECT_SLOW_SQLS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../integration/pdo/pdo.inc');

function test_slow_sql()
{
    global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

    $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, array());
    $result = $conn->query('select * from information_schema.tables limit 1;');
}

test_slow_sql();
