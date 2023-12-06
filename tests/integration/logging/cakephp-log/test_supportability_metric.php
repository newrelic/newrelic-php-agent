<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Supportability metrics: library/cakephp-log/detected and 
Logging/PHP/cakephp-log/disabled metrics are created when cakephp-log 
library is detected.
This test is a mock composer project that uses a logging library as if it
were installed by composer. The library itself is a mock.
*/

/*INI
newrelic.application_logging.enabled = true
*/

/*EXPECT_METRICS_EXIST
Supportability/library/cakephp-log/detected
Supportability/Logging/PHP/cakephp-log/disabled
*/

require_once(realpath(dirname(__FILE__)) . '/vendor/cakephp/log/Log.php');
