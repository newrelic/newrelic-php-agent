<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// Minimal mock of the laravel/framework package that is used to test that the agent
// detects the package, and creates a package version metric as part of special
// instrumentation.

namespace Illuminate\Foundation;

class Application implements \ArrayAccess {
  const VERSION = '9.0.0';
  private $bindings = [];

  function __construct() {}
  function boot() {}
  public function offsetGet($key): mixed {
    return isset($this->bindings[$key]) ? $this->bindings[$key] : null;
  }
  public function offsetExists($key): bool {
    return isset($this->bindings[$key]);
  }
  public function offsetSet($key, $value): void {
    $this->bindings[$key] = $value;
  }
  public function offsetUnset($key): void {
    unset($this->bindings[$key]);
  }
  function bind($key, $value) { $this->bindings[$key] = $value; }
}
