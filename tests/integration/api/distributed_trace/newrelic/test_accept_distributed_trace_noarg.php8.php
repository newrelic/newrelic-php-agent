<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that the API function throws an exception when params are missing.
Prior to PHP 8 this was a warning.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, "8.0", "<")) {
  die("skip: PHP < 8.0.0 not supported\n");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
display_errors=1
log_errors=0
*/

/*EXPECT
Error Detected
Error Detected
*/

$functions = array('newrelic_accept_distributed_trace_payload','newrelic_accept_distributed_trace_payload_httpsafe');
foreach($functions as $function) {
    ob_start();
    try {
      $function();
    } catch (ArgumentCountError $e) {
      echo 'Error Detected',"\n";
    }
}
