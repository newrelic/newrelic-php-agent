<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the agent sends no slow sql transaction when 
Language Agent Security Policy (LASP) configuration
indicates record_sql:{enabled:false}, but agent is configured to send raw SQL.
*/

/*SKIPIF
<?php require('../../integration/pdo/skipif_mysql.inc');
*/

/*INI
newrelic.datastore_tracer.database_name_reporting.enabled = 0
newrelic.datastore_tracer.instance_reporting.enabled = 0
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = "raw"
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
    $result = $conn->query('select * from tables limit 1;');
}

test_slow_sql();
