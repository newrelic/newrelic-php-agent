<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests the Supportability metric:
"Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple"
by calling newrelic_accept_distributed_trace_payload twice.
 */

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"]) || !isset($_ENV["ACCOUNT_supportability_trusted"])) {
    die("skip: env vars required");
}
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_STRICT
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
    [{"name":"DurationByCaller/App/ENV[ACCOUNT_supportability]/ENV[APP_supportability]/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/App/ENV[ACCOUNT_supportability]/ENV[APP_supportability]/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                                                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                                                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/accept_distributed_trace_payload"},                                  [2, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Ignored/Multiple"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/DistributedTrace/AcceptPayload/Success"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/ENV[ACCOUNT_supportability]/ENV[APP_supportability]/Unknown/all"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"TransportDuration/App/ENV[ACCOUNT_supportability]/ENV[APP_supportability]/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},                                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},                                  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




$payload = "{\"v\":[0,1],\"d\":{\"ty\":\"App\",\"ac\":\"{$_ENV['ACCOUNT_supportability']}\",\"ap\":\"{$_ENV['APP_supportability']}\",\"id\":\"3925aa3552e648dd\",\"tr\":\"3925aa3552e648dd\",\"pr\":1.82236,\"sa\":true,\"ti\":1538512769934,\"tk\":\"{$_ENV['ACCOUNT_supportability_trusted']}\"}}";
newrelic_accept_distributed_trace_payload($payload);
newrelic_accept_distributed_trace_payload($payload);
