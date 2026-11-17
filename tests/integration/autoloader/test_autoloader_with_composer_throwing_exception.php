<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used but Composer installation is
broken - one of the methods used by the agent throws an Exception. Supportability
metrics for the Autoloader and Composer libraries should be present: the Composer
files exist, only the Composer API fails. Package harvest should be empty. No
errors should be generated.
*/

/*INI
newrelic.loglevel=verbosedebug
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
null
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
Supportability/library/Composer/detected, 1
*/

/*EXPECT_TRACED_ERRORS null*/

// Simulate autoloader usage:
require 'autoload-with-composer-throwing-exception/vendor/autoload.php';
