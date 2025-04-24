<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

function helper() {
  error_reporting(error_reporting()); // prevent from optimizing this function away
}
