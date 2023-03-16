<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Span events should not be sent when distributed tracing is
disabled, regardless of span events being enabled.
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*INI
zend_extension=opcache.so
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled = 1
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing_enabled=0
error_reporting = E_ALL
opcache.enable=1
opcache.enable_cli=1
opcache.file_update_protection=0
opcache.jit_buffer_size=32M
opcache.jit=tracing
*/

/*EXPECT_SPAN_EVENTS
null
*/

/*EXPECT
Hello
*/

newrelic_add_custom_tracer('main');
function main()
{
  echo 'Hello';
}
main();

