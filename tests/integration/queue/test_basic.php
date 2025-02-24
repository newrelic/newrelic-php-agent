<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should obey the queue time header.
*/

/*INI
*/

/*HEADERS
X_REQUEST_START=1368811467146000
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex"},                                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex/Uri__FILE__"},                                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebFrontend/QueueTime"},                                [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},                           [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                              [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},                  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




echo "hello world";
