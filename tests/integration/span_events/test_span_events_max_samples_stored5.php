<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This is the max that can be sent with DT only.  With span limit > 6800,
infinite tracing NEEDS to be enabled; otherwise, the daemon will
error out with the following type message:
`Error: listener: closing connection: maximum message size exceeded, (2886388 > 2097152)`
The maximum without DT is 7200; however, it causes 
the listener to slow down so much that it will cause intermittent failures on other samples_stored tests.
Only 6800 span events must be sent when the limit is set to 6800.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.span_events.max_samples_stored = 6800
newrelic.code_level_metrics.enabled=false
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 6800
  },
  "??"
]
*/

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

$sample_size = 10000;

for ($i = 0; $i < $sample_size; $i++) {
  main();
}
