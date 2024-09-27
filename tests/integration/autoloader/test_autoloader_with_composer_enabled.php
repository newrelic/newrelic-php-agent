<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used. Supportability metrics for
Autoloader and Composer libraries should be present. Package harvest should
contain all packages reported by composer.
*/

/*INI
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
command=php composer-show.php
expected_packages=vendor1/package1, vendor2/package2
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
Supportability/library/Composer/detected, 1
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-composer/vendor/autoload.php';
