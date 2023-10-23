<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
JIT enabled, should still instrument without crashing.
Previous tests would invariably cause a segfault when
recursively finding the 10th fibonacci number.
 */

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
error_reporting = E_ALL
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
opcache.enable=0
opcache.enable_cli=0
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=function
*/

/*EXPECT_ERROR_EVENTS
null
*/

/*EXPECT
89
*/

newrelic_add_custom_tracer('fibonacci'); 


function fibonacci($n){
    return(($n < 2) ? 1 : fibonacci($n - 2) + fibonacci($n - 1));
}

$n = 10; /* Get the nth Fibonacci number. */

$fibonacci = fibonacci($n);
echo $fibonacci;

