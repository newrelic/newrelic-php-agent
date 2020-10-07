<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic.transaction_events.enabled should control the gathering of transaction
analytics events.
*/

/*INI
newrelic.transaction_events.enabled = 0
*/

/*EXPECT_ANALYTICS_EVENTS
null
*/

