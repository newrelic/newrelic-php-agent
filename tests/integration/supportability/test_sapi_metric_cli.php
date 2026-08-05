<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a Supportability metric for the PHP SAPI name when
running under the CLI SAPI.
*/

/*EXPECT_METRICS_EXIST
Supportability/PHP/SAPI/cli, 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

echo "SAPI: " . php_sapi_name() . "\n";
