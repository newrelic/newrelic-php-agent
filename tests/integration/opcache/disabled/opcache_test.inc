<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test should not run with opache */
if (extension_loaded('Zend OPcache')) {
  if (false != opcache_get_status()) {
    die("fail: OPcache enabled but needs to be disabled\n");
  }
}
