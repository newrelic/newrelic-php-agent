<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Forcing framework to Drupal 8 should generate a framework detection metric, and
enable the Drupal tab in APM.
*/

/*INI
newrelic.framework = drupal8
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"OtherTransaction/all"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},   [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/framework/Drupal8/forced"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

date_default_timezone_set('America/Los_Angeles');
phpinfo();
