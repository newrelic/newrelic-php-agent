<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test detection of autoloader when Composer is not used and use of composer for
package detection is disabled (default). Supportability metrics for
Autoloader and Composer libraries must not be present.
*/

/*INI
*/

/*EXPECT_METRICS_DONT_EXIST
Supportability/library/Autoloader/detected
Supportability/library/Composer/detected
*/

/*EXPECT_TRACED_ERRORS null*/

require 'autoload-without-composer/vendor/autoload.php';
