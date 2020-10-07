<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The PHP agent should not remove any output handler configured in
php.ini.
*/

/*SKIPIF
<?php
if (!extension_loaded('mbstring')) {
  die("skip: mbstring not available");
}
*/

/*INI
output_handler = mb_output_handler
*/

/*EXPECT
array(1) {
  [0]=>
  string(17) "mb_output_handler"
}
*/

var_dump(ob_list_handlers());
