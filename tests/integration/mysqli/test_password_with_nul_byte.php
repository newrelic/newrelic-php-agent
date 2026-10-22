<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A mysqli connect call whose password contains a NUL byte SHALL NOT corrupt
memory. Run it under valgrind.

The agent saves the password of a link for the connection it opens to run
EXPLAIN, and zeroes its own copy after saving it. The copy ends at the first
NUL byte, so zeroing it must not go past that. The test calls the three
wrappers that save a password: mysqli_real_connect(), the constructor and
mysqli_change_user(). The logins are refused (wrong password); the script ends
normally so that valgrind errors fail it.
*/

/*SKIPIF
<?php require("skipif.inc");
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
newrelic.transaction_tracer.explain_enabled = true
newrelic.transaction_tracer.explain_threshold = 0
newrelic.transaction_tracer.record_sql = obfuscated
*/

/*EXPECT
ok - mysqli_real_connect() refused
ok - new mysqli() refused
ok - connected
ok - mysqli_change_user() refused
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

$password = "a\0" . str_repeat("b", 64);

$link = mysqli_init();
tap_assert(!@mysqli_real_connect($link, $MYSQL_HOST, $MYSQL_USER, $password,
                                 $MYSQL_DB, $MYSQL_PORT, $MYSQL_SOCKET),
           'mysqli_real_connect() refused');

$link = @new mysqli($MYSQL_HOST, $MYSQL_USER, $password, $MYSQL_DB,
                    $MYSQL_PORT, $MYSQL_SOCKET);
tap_assert(0 !== $link->connect_errno, 'new mysqli() refused');

$link = mysqli_connect($MYSQL_HOST, $MYSQL_USER, $MYSQL_PASSWD, $MYSQL_DB,
                       $MYSQL_PORT, $MYSQL_SOCKET);
tap_assert($link instanceof mysqli, 'connected');
tap_assert(!@mysqli_change_user($link, $MYSQL_USER, $password, $MYSQL_DB),
           'mysqli_change_user() refused');
