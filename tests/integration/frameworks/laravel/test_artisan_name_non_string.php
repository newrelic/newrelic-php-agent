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
    [{"name":"OtherTransaction/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/Artisan/list"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/Artisan/list"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Laravel/forced"},       [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

use Illuminate\Console\Application;
use Symfony\Component\Console\Input\Input;
use Symfony\Component\Console\Output\Output;

require_once __DIR__.'/mock_artisan.php';

$input = new Input(new stdClass);
$output = new Output;

$app = new Application;
$app->doRun($input, $output);
