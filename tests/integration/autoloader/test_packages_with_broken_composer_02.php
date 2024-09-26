<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of packages when Composer is used but package metadata is bogus:
- package_name is null, package_version is valid
- package_name is valid, package_version is null
- package_name is null, package_version is null.
Supportability metric for Autoloader and Composer libraries should be present because
composer install is not broken. However the agent should create package harvest only
for packages with valid (non null and of type string) package name and version.
No errors should be generated.
*/

/*INI
newrelic.loglevel=verbosedebug
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
command=php composer-show.php packages-with-broken-composer-02/vendor/composer/InstalledVersions.php
expected_packages=laravel/framework
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
Supportability/library/Composer/detected, 1
Supportability/PHP/package/laravel/framework/11/detected
*/

/*EXPECT_METRICS_DONT_EXIST
*/

/*EXPECT_TRACED_ERRORS null*/

// Simulate autoloader usage:
require 'packages-with-broken-composer-02/vendor/autoload.php';
// Simulate package usage (normally this would be done by the autoloader):
include 'packages-with-broken-composer-02/vendor/laravel/framework/src/Illuminate/Foundation/Application.php';
// Trigger instrumentation that generates packages and package-specific metrics:
$app = new Illuminate\Foundation\Application();
