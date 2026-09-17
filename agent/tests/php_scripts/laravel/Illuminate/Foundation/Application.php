<?php

/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace Illuminate\Foundation;

class Application implements \ArrayAccess
{
    public const VERSION = "9.0.0";
    private $bindings = [];

    public function __construct()
    {
    }
    public function boot()
    {
    }

    public function offsetGet($key): mixed
    {
        return isset($this->bindings[$key]) ? $this->bindings[$key] : null;
    }
    public function offsetExists($key): bool
    {
        return isset($this->bindings[$key]);
    }
    public function offsetSet($key, $value): void
    {
        $bindings[$key] = $value;
    }
    public function offsetUnset($key): void
    {
        unset($this->bindings[$key]);
    }
    public function bind($key, $value)
    {
        $this->bindings[$key] = $value;
    }
}
