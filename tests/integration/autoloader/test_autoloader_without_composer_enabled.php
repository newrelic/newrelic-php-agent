<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is not used and use of composer for
package detection is enabled. Supportability metrics for
Autoloader library should be present but Composer should not.
*/

/*INI
newrelic.vulnerability_management.composer_detection.enabled=true
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Composer/detected
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-without-composer/vendor/autoload.php';
