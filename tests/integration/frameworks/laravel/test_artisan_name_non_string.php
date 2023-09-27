<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Running an artisan command with a bad command name should result in the
transaction being named "list".
*/

/*INI
newrelic.framework=laravel
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/Artisan/list"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/Artisan/list"},                 [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Laravel/forced"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




use Illuminate\Console\Application;
use Symfony\Component\Console\Input\Input;
use Symfony\Component\Console\Output\Output;

if (version_compare(PHP_VERSION, "8.0", ">=")){
  require_once __DIR__.'/mock_artisan.php8.php';
} else {
  require_once __DIR__.'/mock_artisan.php';
}

$input = new Input(new stdClass);
$output = new Output;

$app = new Application;
$app->doRun($input, $output);
