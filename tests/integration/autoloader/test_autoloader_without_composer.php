<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is not used. Supportability metrics for
Autoloader library should be present but Composer should not.
*/

/*INI
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Composer/detected
*/

require 'autoload-without-composer/vendor/autoload.php';
