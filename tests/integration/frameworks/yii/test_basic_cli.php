<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should name Yii2 cli transactions
*/

/*EXPECT_METRICS_EXIST
Supportability/framework/Yii2/detected
OtherTransaction/Action/test-integration-cli-action
*/

/*EXPECT_ERROR_EVENTS null */

/* Yii2 mock */
require_once __DIR__.'/yii2/baseyii.php';

$a = new yii\base\InlineAction('test-integration-cli-action');
$a->runWithParams([]);
