<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/polyfill.php');
require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

// Wrap a Memcached object and provide a convenient way to exercise
// its methods while asserting the expected result. When an operation
// fails, print the Memcached error message to aid debugging.
if (version_compare(PHP_VERSION, '8.2', '>=')) {
  class BaseTestCase
  {
    private $memcached;
    function __construct($memcached) {
      $this->memcached = $memcached;
    }

    // Decorate Memcached methods that return a boolean indicating success
    // or failure. The method name can be prefixed with 'assert' to
    // expect the operation to succeed, or 'refute' to expect the operation
    // to fail.
    function __call($method, $args) {
      if ('refute' === substr($method, 0, 6)) {
        return $this->refute(lcfirst(substr($method, 6)), $args);
      }

      if ('assert' === substr($method, 0, 6)) {
        return $this->assert(lcfirst(substr($method, 6)), $args);
      }

      return $this->assert($method, $args);
    }

    // Execute the Memcached operation given by $method with $args and expect
    // it to return a truthy (non-false) value.
    function assert($method, $args) {
      $actual = call_user_func_array(array($this->memcached, $method), $args);
      if (FALSE !== $actual) {
        tap_ok($method);
      } else {
        tap_not_ok($method, TRUE, FALSE);
        tap_diagnostic("getResultMessage()=" . $this->memcached->getResultMessage());
      }
      return $actual;
    }

    // Execute the Memcached operation given by $method with $args and expect
    // it to return FALSE.
    function refute($method, $args) {
      $actual = call_user_func_array(array($this->memcached, $method), $args);
      if (FALSE === $actual) {
        tap_ok($method);
      } else {
        tap_not_ok($method, FALSE, $actual);
        tap_diagnostic("getResultMessage()=" . $this->memcached->getResultMessage());
      }
      return $actual;
    }

    // Assert the value of $key is equal to $expected. If $key is an array
    // then the values are retrieved using getMulti; otherwise, get is used.
    function assertEqual($expected, $key) {
      if (is_array($key)) {
        $actual = $this->memcached->getMulti($key);
        tap_equal_unordered($expected, $actual, __FUNCTION__);
      } else {
        tap_equal($expected, $this->memcached->get($key), __FUNCTION__);
      }
    }

    // Assert the value of $key is equal to $expected.
    function assertEqualByKey($expected, $server_key, $key) {
      if (is_array($key)) {
        $actual = $this->memcached->getMultiByKey($server_key, $key);
        tap_equal_unordered($expected, $actual, __FUNCTION__);
      } else {
        $actual = $this->memcached->getByKey($server_key, $key);
        tap_equal($expected, $actual, __FUNCTION__);
      }
    }
  }
}
else {
  class BaseTestCase
  {
    function __construct($memcached) {
      $this->memcached = $memcached;
    }

    // Decorate Memcached methods that return a boolean indicating success
    // or failure. The method name can be prefixed with 'assert' to
    // expect the operation to succeed, or 'refute' to expect the operation
    // to fail.
    function __call($method, $args) {
      if ('refute' === substr($method, 0, 6)) {
        return $this->refute(lcfirst(substr($method, 6)), $args);
      }

      if ('assert' === substr($method, 0, 6)) {
        return $this->assert(lcfirst(substr($method, 6)), $args);
      }

      return $this->assert($method, $args);
    }

    // Execute the Memcached operation given by $method with $args and expect
    // it to return a truthy (non-false) value.
    function assert($method, $args) {
      $actual = call_user_func_array(array($this->memcached, $method), $args);
      if (FALSE !== $actual) {
        tap_ok($method);
      } else {
        tap_not_ok($method, TRUE, FALSE);
        tap_diagnostic("getResultMessage()=" . $this->memcached->getResultMessage());
      }
      return $actual;
    }

    // Execute the Memcached operation given by $method with $args and expect
    // it to return FALSE.
    function refute($method, $args) {
      $actual = call_user_func_array(array($this->memcached, $method), $args);
      if (FALSE === $actual) {
        tap_ok($method);
      } else {
        tap_not_ok($method, FALSE, $actual);
        tap_diagnostic("getResultMessage()=" . $this->memcached->getResultMessage());
      }
      return $actual;
    }

    // Assert the value of $key is equal to $expected. If $key is an array
    // then the values are retrieved using getMulti; otherwise, get is used.
    function assertEqual($expected, $key) {
      if (is_array($key)) {
        $actual = $this->memcached->getMulti($key);
        tap_equal_unordered($expected, $actual, __FUNCTION__);
      } else {
        tap_equal($expected, $this->memcached->get($key), __FUNCTION__);
      }
    }

    // Assert the value of $key is equal to $expected.
    function assertEqualByKey($expected, $server_key, $key) {
      if (is_array($key)) {
        $actual = $this->memcached->getMultiByKey($server_key, $key);
        tap_equal_unordered($expected, $actual, __FUNCTION__);
      } else {
        $actual = $this->memcached->getByKey($server_key, $key);
        tap_equal($expected, $actual, __FUNCTION__);
      }
    }
  }
}

if (version_compare(PHP_VERSION, '7.0', '>=')) {
  // PHP 7 doesn't need the special case handling, as the API no longer has
  // by-ref parameters.
  class TestCase extends BaseTestCase {}
} else {
  class TestCase extends BaseTestCase
  {
    // Perform a Memcached get operation. This is a simple pass-through because
    // get does not return a boolean value, and because $cas_token is passed by
    // reference.
    function get($key, $cache_cb = NULL, &$cas_token = NULL) {
      return $this->memcached->get($key, $cache_cb, $cas_token);
    }

    // Perform a Memcached getByKey operation. This is a simple pass-through
    // because getByKey does not return a boolean value, and because $cas_token
    // is passed by reference.
    function getByKey($server_key, $key, $cache_cb = NULL, &$cas_token = NULL) {
      return $this->memcached->getByKey($server_key, $key, $cache_cb, $cas_token);
    }
  }
}
