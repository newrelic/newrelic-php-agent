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
newrelic.span_events.max_samples_stored = 0
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 2000
  },
  "??"
]
 */

$NEWRELIC_SPAN_EVENTS_MAX = 3000; // The number of span events to send per transaction. 
				  //The agent internal maximum is 10000.

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

for ($i = 0; $i < $NEWRELIC_SPAN_EVENTS_MAX + 1; $i++) {
  main();
}
