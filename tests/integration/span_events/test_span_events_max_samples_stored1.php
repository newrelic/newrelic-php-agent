<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Only 101 span events must be sent when the limit is set to 101.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.span_events.max_samples_stored = 101
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 101
  },
  "??"
]
*/

newrelic_add_custom_tracer('main');
function main()
{
  usleep(1);
}

$sample_size = 10000;

for ($i = 0; $i < $sample_size; $i++) {
  main();
}
