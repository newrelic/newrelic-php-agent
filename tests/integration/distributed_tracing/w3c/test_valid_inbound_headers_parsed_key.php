<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metric "Supportability/DistributedTrace/AcceptPayload/Success"
when the payload is correct.
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability_trusted"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled = true
newrelic.cross_application_tracer.enabled = false
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"DurationByCaller/App/1349956/41346604/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/App/1349956/41346604/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Forwarding/PHP/disabled"},    [1, "??", "??", "??", "??", "??"]],
    [{"name": "Supportability/Logging/Metrics/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/TraceContext/Accept/Success"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_headers"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/1349956/41346604/Unknown/all"},
                                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/1349956/41346604/Unknown/allOther"},
                                                          [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

$payload = array(
  'HtTp_tRacepArent' => "00-74be672b84ddc4e4b28be285632bbc0a-27ddd2d8890283b4-01",
  'hTtP_trAceState' => "{$_ENV['ACCOUNT_supportability_trusted']}@nr=0-0-1349956-41346604-27ddd2d8890283b4-b28be285632bbc0a-1-1.1273-1569367663277"
);

newrelic_accept_distributed_trace_headers($payload);
