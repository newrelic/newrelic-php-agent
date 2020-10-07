<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_ignore_transaction should not record a transaction, resulting in no harvest
*/

/*EXPECT_HARVEST no */

newrelic_ignore_transaction();
