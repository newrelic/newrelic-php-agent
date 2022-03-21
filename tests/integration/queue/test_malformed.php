<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should obey the queue time header.
*/

/*HEADERS
X_REQUEST_START=abc
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"},
                                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allWeb"},
                                                     [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"Apdex/Uri__FILE__"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"HttpDispatcher"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction"},                      [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransaction/Uri__FILE__"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime"},             [1, "??", "??", "??", "??", "??"]],
    [{"name":"WebTransactionTotalTime/Uri__FILE__"}, [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

echo "hello world";
