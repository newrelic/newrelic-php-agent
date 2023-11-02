<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not cause a deprecation warning to be thrown when passing null
to optional arguments for mysqli::__construct on PHP 8.1+
*/

/*SKIPIF
<?php require("skipif.inc");

if (version_compare(PHP_VERSION, "8.1", "<")) {
  die("skip: Deprecation warning PHP 8.1+ specific\n");
}
*/

/*INI
error_reporting = E_DEPRECATION
*/

/*EXPECT
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

$link = new mysqli(null, null, null, null, null, null);

if (mysqli_connect_errno()) {
  echo mysqli_connect_error() . "\n";
  exit(1);
}

mysqli_close($link);
