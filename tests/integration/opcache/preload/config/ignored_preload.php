<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

newrelic_end_transaction(true);
require __DIR__ . '/../vendor/mongodb/mongodb/src/Client.php';
