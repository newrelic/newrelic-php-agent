<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Span events should not be sent when distributed tracing is
disabled, regardless of span events being enabled.
*/

/*INI
newrelic.transaction_tracer.threshold = 0
newrelic.span_events_enabled = 1
newrelic.cross_application_tracer.enabled = false
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

