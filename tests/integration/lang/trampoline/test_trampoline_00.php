<?php
/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that trampoline functions not blow up.
*/

/*INI
newrelic.code_level_metrics.enabled = true
*/

/*EXPECT_TRACED_ERRORS null*/

/*EXPECT
bool(false)
*/

class B {}
class A extends B
{
  public function bar($func)
  {
    var_dump(is_callable(array('B', 'foo')));
  }

  public function __call($func, $args) {}
}

class X
{
  public static function __callStatic($func, $args) {}
}

$a = new A();
// Extra X::foo() wrapper to force use of allocated trampoline.
X::foo($a->bar('foo'));
