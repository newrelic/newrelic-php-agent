<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test autoloader detection when Composer api is enabled but package detection
is disabled. Autoloader and Composer must not be detected. Supportability
metrics for Autoloader and Composer libraries should not be present. Package
harvest should be empty. No errors should be generated.
*/

/*INI
newrelic.vulnerability_management.package_detection.enabled=false
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
null
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Autoloader/detected
Supportability/library/Composer/detected
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-composer/vendor/autoload.php';
