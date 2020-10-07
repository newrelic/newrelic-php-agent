<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that the API function warns when params are missing.
*/

/*INI
display_errors=1
log_errors=0
*/

/*EXPECT
Warning Detected
Warning Detected
*/

$functions = array('newrelic_accept_distributed_trace_payload','newrelic_accept_distributed_trace_payload_httpsafe');
foreach($functions as $function) {
    ob_start();
    $function();
    $contents = ob_get_clean();
    if(strpos($contents, 'Warning:') !== false)
    {
        echo 'Warning Detected',"\n";
    }
}
