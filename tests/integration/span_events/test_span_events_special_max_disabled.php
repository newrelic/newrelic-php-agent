<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */
/*DESCRIPTION
1000 span events must be sent when the limit is disabled.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.special.max_span_events = 0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 1000,
    "events_seen": 1000 
  },
  "??"
]
 */

$NEWRELIC_SPAN_EVENTS_MAX = 1000; // The agent internal maximum of span events 
				  // per transaction.

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

for ($i = 0; $i < $NEWRELIC_SPAN_EVENTS_MAX + 1; $i++) {
  main();
}
