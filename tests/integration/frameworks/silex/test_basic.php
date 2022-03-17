<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should name Silex transactions that have _route attributes.
*/

/*INI
newrelic.distributed_tracing_enabled=0
*/

/*SKIPIF <?php require('skipif.inc'); */

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"OtherTransaction/all"},                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/Action/GET_foo"}, [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/Action/GET_foo"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Silex/detected"},  [1,    0,    0,    0,    0,    0]]
  ]
]
*/

use Symfony\Component\HttpFoundation\ParameterBag;
use Symfony\Component\HttpFoundation\Request;
use Symfony\Component\HttpKernel\HttpKernel;

include __DIR__.'/Silex/Application.php';

$kernel = new HttpKernel;
$request = new Request;
$request->attributes = new ParameterBag;
$request->attributes->_route = 'GET_foo';

$kernel->handle($request);
