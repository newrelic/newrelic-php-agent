<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When the span events max_samples_stored limit is set to any integer 
less than 1 or greater than 10000(max value)
the max_samples_stored value should be treated as the default (2000) value.
Setting a invalid value should result in the default value of 2000 being seen.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.span_events.max_samples_stored = 0.4
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
