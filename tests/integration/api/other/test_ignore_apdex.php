<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that no apdex metrics are created after calling newrelic_ignore_apdex.
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"HttpDispatcher"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"},    [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/api/background_job"},      [1, 0, 0, 0, 0, 0]],
    [{"name":"Supportability/api/ignore_apdex"},        [1, 0, 0, 0, 0, 0]]
  ]
]
*/

/* Ensure that this is a web transaction. */
newrelic_background_job(false);

newrelic_ignore_apdex();
