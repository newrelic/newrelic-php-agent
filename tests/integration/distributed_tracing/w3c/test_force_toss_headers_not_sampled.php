<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metric "Supportability/DistributedTrace/AcceptPayload/Success"
when the payload is correct.
 */

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.distributed_tracing.sampler.remote_parent_not_sampled = 'always_off'
*/

/*EXPECT_SPAN_EVENTS
null
*/




$payload = array(
  'traceparent' => "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-00",
  'tracestate' => "123@nr=0-0-1349956-41346604-27ddd2d8890283b4-b28be285632bbc0a-1-1.1273-1569367663277"
);

newrelic_accept_distributed_trace_headers($payload);
