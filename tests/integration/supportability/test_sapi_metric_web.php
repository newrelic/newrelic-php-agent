<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should record a Supportability metric for the PHP SAPI name when
running under the CGI SAPI. Declaring REQUEST_METHOD forces the
integration runner to invoke this test via php-cgi (Test.IsWeb() ==
true) instead of the plain php CLI binary, so the agent observes
sapi_module.name == "cgi-fcgi" rather than "cli".
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT_METRICS_EXIST
Supportability/PHP/SAPI/cgi-fcgi, 1
*/

/*EXPECT_TRACED_ERRORS
null
*/

echo "SAPI: " . php_sapi_name() . "\n";
