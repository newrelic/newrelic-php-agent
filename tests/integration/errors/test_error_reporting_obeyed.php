<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should not capture errors which are are not part of the
error_reporting directive bitset.
*/

/*EXPECT_TRACED_ERRORS
null
*/

// PHP 7 greatly reduced the number of "true" errors: the vast majority of
// runtime errors are now thrown as exceptions. This is a rare case that's a
// runtime error from PHP 5.0 onwards, provided the error occurs at runtime
// (hence why the class definitions are in a function; this makes them
// conditional and avoids the error being generated at compile time).

function generate_error() {
  class A {
    final public function f() {}
  }

  class B extends A {
    public function f() {}
  }
}

error_reporting(0);

generate_error();
