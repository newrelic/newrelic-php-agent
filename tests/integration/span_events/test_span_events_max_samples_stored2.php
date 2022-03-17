<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
When sending 5 span events, 5(+ 1 transactional)  must be sent when the limit is set to greater than 5.
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.transaction_tracer.threshold = 0
newrelic.cross_application_tracer.enabled = false
newrelic.span_events.max_samples_stored = 10000
*/

/*EXPECT_SPAN_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": 10000,
    "events_seen": 6
  },
  "??"
]
*/

newrelic_add_custom_tracer('main');
function main()
{
  usleep(10);
}

$sample_size = 5;

for ($i = 0; $i < $sample_size; $i++) {
  main();
}
