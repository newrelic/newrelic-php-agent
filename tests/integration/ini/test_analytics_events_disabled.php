<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic.analytics_events.enabled should control the gathering of transaction
analytics events.  This setting has been deprecated in favor of
newrelic.transaction_events.enabled, but it is still observed.
*/

/*INI
newrelic.analytics_events.enabled = 0
*/

/*EXPECT_ANALYTICS_EVENTS
null
*/

