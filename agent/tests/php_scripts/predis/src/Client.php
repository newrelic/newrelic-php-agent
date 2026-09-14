<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// Minimal mock of the predis/predis package that is used to test that the agent
// detects the package, and creates a package version metric as part of special
// instrumentation. The mock is used by agent/tests/test_php_observer.c.

namespace Predis;

// Implementing an interface (rather than adding a top-level executable statement)
// ensures this file is not marked as empty by opcache's preload_remove_empty_includes()
// (ext/opcache/ZendAccelerator.c) and thus the file is actually executed when pulled in
// by require(). This allows the agent's hook that detects libraries (nr_php_execute_file)
// to be triggered.
//
// Note: a bare top-level statement (e.g. a stray class_exists() call) would work too,
// but files in real-world PHP packages rarely contain one. Implementing an interface,
// on the other hand, is completely ordinary. This mock therefore stays truer to what
// it's standing in for.
interface MarkerInterface {}

class Client implements MarkerInterface {
    // __construct() method is required to trigger testing conditions in test_php_observer.c,
    // i.e. executing special instrumentation which generates package version metric.
    public function __construct() {
        echo "Predis\\Client::__construct called\n";
    }
    // VERSION constant is required to trigger testing conditions in test_php_observer.c,
    // i.e. package version metric creation. The value is not important, just needs to be present.
    public const VERSION = '1.1.10';
}
