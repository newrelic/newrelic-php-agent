<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Supportability metrics: library/Monolog/detected and 
Logging/PHP/Monolog/enabled metrics are created when the MonoLog 
library is detected.
This test is a mock composer project that uses a logging library as if it
were installed by composer. The library itself is a mock.
*/

/*INI
newrelic.application_logging.enabled = true
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Monolog/detected
Supportability/Logging/PHP/Monolog/enabled
*/

require_once(realpath(dirname(__FILE__)) . '/vendor/Monolog/Logger.php');
