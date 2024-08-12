
<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Verify the PHP version and agent version metrics are created properly.
*/

/*EXPECT_METRICS_EXIST
Supportability/PHP/Version/ENV[PHP_VERSION], 1
Supportability/PHP/AgentVersion/ENV[AGENT_VERSION], 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

if (!extension_loaded('newrelic')) {
    die("fail: New Relic PHP extension is not loaded. Exiting...\n");
    exit;
}

echo "PHP Version: " . phpversion() . "\n";
echo "Agent Version: " . newrelic_get_agent_version() . "\n";
