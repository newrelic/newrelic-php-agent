<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the span events max_samples_stored limit is set to any integer 
less than 1 or greater than 10000(max value)
the max_samples_stored value should be treated as the default (2000) value.
Setting a value of 3000 should result in the value 3000 being seen
even if CAT is enabled (which should be ignored).
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = true
newrelic.span_events.max_samples_stored = 3000
newrelic.code_level_metrics.enabled=false
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 3000
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
