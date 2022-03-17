<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Running an artisan command with an unexpected input interface argument should
result in the transaction being named "unknown".
*/

/*INI
newrelic.framework = laravel
newrelic.distributed_tracing_enabled=0
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/unknown"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/unknown"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Laravel/forced"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

use Illuminate\Console\Application;
use Symfony\Component\Console\Output\Output;

require_once __DIR__.'/mock_artisan.php';

$output = new Output;

$app = new Application;
$app->doRun(true, $output);
