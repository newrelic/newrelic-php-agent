<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A slow query on a view SHALL succeed with no error when the MySQL user may read
the view but not the table behind it.

MySQL lets such a user query the view, but refuses EXPLAIN on it ("EXPLAIN/SHOW
can not be issued; lacking privileges for underlying table"). The agent runs
EXPLAIN on its own connection with the same user, so the EXPLAIN is refused. On
PHP 8.1 and later mysqli throws mysqli_sql_exception for that, and the
exception must not reach the application's query.

The user, table and view are in tests/tls/init.sql.
The test is skipped only if MYSQL_TLS_HOST is not set or PHP is below 7.4;
an unusable server or user fails it.
*/

/*SKIPIF
<?php
require("skipif.inc");
$tls_profile = 'view_only';
require("skipif_tls.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
1
1
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$link = mysqli_connect($MYSQL_TLS_HOST, 'nr_view_only', 'nr_view_only',
                       $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT);

/* The query entry point. */
$result = mysqli_query($link, "SELECT id FROM nr_view_priv_v WHERE id = 1");
while ($row = mysqli_fetch_row($result)) {
  echo $row[0] . "\n";
}
mysqli_free_result($result);

/* The prepared statement entry point. */
$stmt = mysqli_prepare($link, "SELECT id FROM nr_view_priv_v WHERE id = 1");
mysqli_stmt_execute($stmt);
mysqli_stmt_bind_result($stmt, $value);
while (mysqli_stmt_fetch($stmt)) {
  echo $value . "\n";
}
mysqli_stmt_close($stmt);

mysqli_close($link);
