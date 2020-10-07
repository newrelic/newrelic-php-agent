<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Polyfill various PHP APIs so tests can program to a consistent set
 * regardless of the version of PHP.
 */

if (FALSE === function_exists('lcfirst')) {
  /**
   * Returns a string with the first character of $str, lowercased if
   * that character is alphabetic.
   *
   * @param string $str The input string.
   *
   * @return string
   */
  function lcfirst($str)
  {
    return strtolower(substr($str, 0, 1)) . substr($str, 1);
  }
}

if (FALSE === function_exists('getallheaders')) {
  /**
   * Returns all headers sent in the HTTP request.
   *
   * @return array
   */
  function getallheaders()
  {
    $headers = array();
    foreach ($_SERVER as $k => $v) {
      if (substr($k, 0, 5) == 'HTTP_') {
        $name = strtolower(substr($k, 5));
        $name = implode('-', array_map('ucfirst', explode('_', $name)));
        $headers[$name] = $v;
      }
    }
    return $headers;
  }
}
