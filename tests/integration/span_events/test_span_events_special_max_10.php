<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Only 10 span events must be sent when the limit is set to 10.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.special.max_span_events = 10
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 10
  },
  "??"
]
*/

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

for ($i = 0; $i < 200; $i++) {
  main();
}
