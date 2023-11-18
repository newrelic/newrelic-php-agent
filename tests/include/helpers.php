<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Generate a random string consisting of characters. This is useful for
 * generating unique names.  The default charset does not contain digits
 * to ensure that we don't return only digits:  Memcache may coerce
 * digit-only string keys into numbers.
 */
function randstr($length, $charset='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ') {
  $str = '';
  $count = strlen($charset);
  while ($length--) {
    $str .= $charset[mt_rand(0, $count-1)];
  }
  return $str;
}

/*
 * This is in a helper function to avoid stack differences
 * between HHVM and Zend.
 */
function force_error() {
  newrelic_notice_error("HACK: forced error");
}

/*
 * A user function has to be called to force a transaction trace. PHP 7.1
 * includes dead code elimination, which means that we have to actually do
 * something that can't be eliminated: re-setting the error reporting level to
 * what it already is will do the job. Additionally force non-zero duration for the segment not to be dropped.
 */
function force_transaction_trace() {
  error_reporting(error_reporting()); time_nanosleep(0, 50000); // 50usec should be enough
}
