<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_disable_autorum() should prevent RUM insertion.
*/

/*ENVIRONMENT
REQUEST_METHOD=GET
*/

/*EXPECT
<html>
<head></head>
<body></body>
</html>
*/

newrelic_disable_autorum();
?>
<html>
<head></head>
<body></body>
</html>
