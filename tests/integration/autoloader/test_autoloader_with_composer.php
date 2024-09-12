<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used. Supportability metrics for
Autoloader and Composer libraries should be present. Additionally, package
supportability metrics should be present for each package detected.
*/

/*INI
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
Supportability/library/Composer/detected, 1
Supportability/PHP/package/vendor1/package1/1/detected, 1
Supportability/PHP/package/vendor2/package2/2/detected, 1
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-composer/vendor/autoload.php';
