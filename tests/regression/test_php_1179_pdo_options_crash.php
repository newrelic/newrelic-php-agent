<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not crash or cause PHP to crash when explain plans are
generated.
*/


/*
 * Disable reporting of warnings in the SKIPIF bloc. 
 * Despite what the PHP manual says,
 * PDO::__construct seems to unconditionally trigger a warning on
 * connect failure. Setting the error mode to PDO::ERRMODE_SILENT or
 * PDO::ERRMODE_EXCEPTION only affects the behavior of the PDO object
 * after the connection is established. We need to suppress the warning
 * to ensure the skip message is the first line of output,
 * and it can be observed by the test runner.
 */
/*SKIPIF
<?php

if (!extension_loaded('pdo')) {
  die("skip: pdo extension required\n");
}

if (!extension_loaded('pdo_mysql')) {
  die("skip: pdo_mysql extension required\n");
}

require_once(realpath(dirname(__FILE__)) . '/../include/config.php');

$level = error_reporting();
error_reporting($level & ~E_WARNING);

try {
  $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD);
} catch (Exception $e) {
  die("skip: " . $e->getMessage() . "\n");
}
*/

/*INI
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
*/

/*EXPECT
Look Ma, no crashes!
*/

require_once(realpath(dirname(__FILE__)) . '/../include/config.php');

function test_slow_sql(array $options)
{
    global $PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD;

    $conn = new PDO($PDO_MYSQL_DSN, $MYSQL_USER, $MYSQL_PASSWD, $options);
    $result = $conn->query('select * from information_schema.tables limit 1;');
}

$shared_options = array(PDO::ATTR_PERSISTENT => true);

test_slow_sql($shared_options);
test_slow_sql(array());
test_slow_sql(array());
test_slow_sql($shared_options);

echo "Look Ma, no crashes!\n";
