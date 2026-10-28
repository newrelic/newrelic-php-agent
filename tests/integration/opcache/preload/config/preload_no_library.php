<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// Don't report the transaction the agent starts for the preload request.
newrelic_end_transaction(true);

// Preload nothing but this script, which matches no signature file.
