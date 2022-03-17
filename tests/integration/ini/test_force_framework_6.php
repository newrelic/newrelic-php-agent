<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Forcing the framework to 'none' should not generate a forced framework metric.
*/

/*INI
newrelic.framework=none
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"},
                                                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"CPU/User Time"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"CPU/User/Utilization"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Memory/Physical"},                        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/execute/user/call_count"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

date_default_timezone_set('America/Los_Angeles');
phpinfo();
