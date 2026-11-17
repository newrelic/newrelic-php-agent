<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A null host, user or password given to a connect call SHALL be replaced by the
mysqli.default_* settings, as PHP does without the agent.

From PHP 7.4 PHP replaces a null host, user or password by the settings. If the
agent parsed the arguments with a specification that turns a null into an empty
string, mysqli would receive the empty string and log in as the empty user while
a transaction is recorded.
*/

/*SKIPIF
<?php
require("skipif.inc");
if (PHP_VERSION_ID < 70400) {
  die("skip: a null user uses mysqli.default_user from PHP 7.4\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED
*/

/*EXPECT
ok - mysqli_real_connect() with null arguments
ok - mysqli_connect() with null arguments
ok - new mysqli() with null arguments
ok - mysqli::real_connect() with null arguments
*/

/*EXPECT_TRACED_ERRORS
null
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/tap.php');
require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

mysqli_report(MYSQLI_REPORT_OFF);

ini_set('mysqli.default_host', $MYSQL_HOST);
ini_set('mysqli.default_user', $MYSQL_USER);
ini_set('mysqli.default_pw', $MYSQL_PASSWD);

$link = mysqli_init();
tap_assert(@mysqli_real_connect($link, null, null, null, $MYSQL_DB,
                                (int)$MYSQL_PORT),
           'mysqli_real_connect() with null arguments');
mysqli_close($link);

$link = @mysqli_connect(null, null, null, $MYSQL_DB, (int)$MYSQL_PORT);
tap_assert($link instanceof mysqli, 'mysqli_connect() with null arguments');
mysqli_close($link);

$link = @new mysqli(null, null, null, $MYSQL_DB, (int)$MYSQL_PORT);
tap_assert(0 === $link->connect_errno, 'new mysqli() with null arguments');
$link->close();

$link = mysqli_init();
tap_assert(@$link->real_connect(null, null, null, $MYSQL_DB, (int)$MYSQL_PORT),
           'mysqli::real_connect() with null arguments');
$link->close();
