<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */


/*DESCRIPTION
Exercise the wrappers for the various output buffer functions.
*/

/*INI
error_reporting = E_ERROR | E_CORE_ERROR | E_RECOVERABLE_ERROR
newrelic.application_logging.enabled = false
newrelic.application_logging.forwarding.enabled = false
newrelic.application_logging.metrics.enabled = false
*/

/*EXPECT
junk0
junk1
junk3
junk4
junk5
junk6
block0 <br>
block1 <br>
block2 <br>
block3 <br>
block4 <br>
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
    [{"name":"OtherTransaction/php__FILE__"},                         [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/disabled"},       [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/disabled"},          [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},  [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




echo "junk0\n"; ob_end_clean ();
echo "junk1\n"; ob_end_flush ();

echo "junk3\n"; ob_get_clean ();
echo "junk4\n"; ob_get_flush ();
echo "junk5\n"; ob_clean ();
echo "junk6\n"; flush ();

echo "block0 <br>\n"; ob_flush ();
echo "block1 <br>\n"; ob_flush ();
echo "block2 <br>\n"; ob_flush ();
echo "block3 <br>\n"; ob_flush ();
echo "block4 <br>\n"; ob_flush ();
