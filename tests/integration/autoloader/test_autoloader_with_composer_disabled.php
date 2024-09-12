<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used and use of composer for
package detection is disabled (default). Supportability metrics for
Autoloader and Composer libraries metrics must not be present. Additionally,
package supportability metrics and package harvest must not be present.
*/

/*INI
*/

/*EXPECT_PHP_PACKAGES null*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Autoloader/detected
Supportability/library/Composer/detected
Supportability/PHP/package/vendor1/package1/1/detected
Supportability/PHP/package/vendor2/package2/2/detected
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-composer/vendor/autoload.php';
