<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that no apdex metrics are created after calling newrelic_ignore_apdex.
*/

/*INI
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/background_job"},                    [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/ignore_apdex"},                      [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




/* Ensure that this is a web transaction. */
newrelic_background_job(false);

newrelic_ignore_apdex();
