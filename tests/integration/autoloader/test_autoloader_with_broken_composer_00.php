<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used but Composer installation
is broken - method is unavailable. Supportability metric for Autoloader
library should be present but not for Composer library. Package harvest should
be empty. No errors should be generated.
*/

/*INI
newrelic.vulnerability_management.composer_api.enabled=true
*/

/*EXPECT_PHP_PACKAGES
null
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Composer/detected, 1
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-broken-composer-00/vendor/autoload.php';
