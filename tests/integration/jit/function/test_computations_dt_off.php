<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Span events should not be sent when distributed tracing is
enabled, span events are disabled and cat is enabled.
*/

/*INI
newrelic.distributed_tracing_enabled=1
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled=0
newrelic.cross_application_tracer.enabled = true
error_reporting = E_ALL
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=function
*/

/*EXPECT_SPAN_EVENTS
null
*/

/*EXPECT
Hello
*/

newrelic_add_custom_tracer('computation');

function computation(float $a): float
{
    $b = $a % (2 ** 32);
    return $b;
}

for ($i = 0; $i < 500; ++$i) {
    computation(2**64);
}
echo 'Hello';