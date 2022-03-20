<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the span events max_samples_stored limit is set to any non-integer
the max_samples_stored value should be treated as the default (2000) value.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.span_events.max_samples_stored = a
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

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

$sample_size = 10000;

for ($i = 0; $i < $sample_size; $i++) {
  main();
}
