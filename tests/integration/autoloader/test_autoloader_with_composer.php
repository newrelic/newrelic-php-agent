<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is used. Supportability metrics for
Autoloader and Composer libraries should be present.
*/

/*INI
*/

/*EXPECT_METRICS_EXIST
Supportability/library/Autoloader/detected, 1
Supportability/library/Composer/detected, 1
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-with-composer/vendor/autoload.php';
