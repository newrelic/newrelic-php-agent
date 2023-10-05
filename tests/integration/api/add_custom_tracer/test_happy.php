<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test normal successful usage of newrelic_add_custom_tracer.
*/

/*INI
*/

/*EXPECT
zip
zap
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? timeframe start",
  "?? timeframe stop",
  [
   [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/all"}, [1, "??", "??", "??", "??", "??"]],
   [{"name":"DurationByCaller/Unknown/Unknown/Unknown/Unknown/allOther"}, [1, "??", "??", "??", "??", "??"]],
   [{"name":"Custom/MY_function"},                                   [1, "??", "??", "??", "??", "??"]],
   [{"name":"Custom/MY_function",
     "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
   [{"name":"Custom/MY_class::MY_method"},                           [1, "??", "??", "??", "??", "??"]],
   [{"name":"Custom/MY_class::MY_method",
     "scope":"OtherTransaction/php__FILE__"},                        [1, "??", "??", "??", "??", "??"]],
   [{"name":"OtherTransaction/all"},                                 [1, "??", "??", "??", "??", "??"]],
   [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
   [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
   [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
   [{"name":"Supportability/api/add_custom_tracer"},                 [2, "??", "??", "??", "??", "??"]],
   [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},        [1, "??", "??", "??", "??", "??"]],
   [{"name":"Supportability/Logging/Metrics/PHP/enabled"},           [1, "??", "??", "??", "??", "??"]],
   [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




function MY_function($x) {
    echo $x;
}

class MY_class {
    public static function MY_method($x) {
        echo $x;
    }
}

/*
 * Note that capitalization has been changed to test case insensitive lookup.
 *
 * Note that the metrics contain the capitalization of the actual code, not
 * the parameters we are given.
 */
newrelic_add_custom_tracer("my_FUNCTION");
newrelic_add_custom_tracer("my_CLASS::my_METHOD");

MY_function("zip\n");
MY_class::MY_method("zap\n");
