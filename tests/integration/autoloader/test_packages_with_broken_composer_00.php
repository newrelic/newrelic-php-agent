<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of packages when Composer is used but Composer installation
is broken - method is unavailable. Supportability metric for Autoloader
library should be present but not for Composer library. Package harvest should
not be empty but generated using legacy method when package version is unknown.
Because package version is unknown, no package-specific metrics should be generated.
No errors should be generated.
*/

/*INI
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
command=php composer-show.php packages-with-broken-composer-00/vendor/composer/InstalledVersions.php symfony/http-kernel
expected_packages=symfony/http-kernel
package_name_only=symfony/http-kernel
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Composer/detected, 1
Supportability/PHP/package/symfony/http-kernel/5/detected
*/

/*EXPECT_TRACED_ERRORS null*/

// Simulate autoloader usage:
require 'packages-with-broken-composer-00/vendor/autoload.php';
// Simulate package usage (normally this would be done by the autoloader):
include 'packages-with-broken-composer-00/vendor/symfony/http-kernel/HttpKernel.php';
