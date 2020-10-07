<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_notice_error should not record a traced error if the agent is not
currently recording.
*/

/*EXPECT_TRACED_ERRORS null */
/*EXPECT_ERROR_EVENTS null */

newrelic_ignore_transaction();
newrelic_notice_error("don't report me bro");
newrelic_end_transaction();

// Start a new transaction to ensure some data is added to the harvest.
// This is required by the integration test runner.
newrelic_start_transaction(ini_get("newrelic.appname"));
