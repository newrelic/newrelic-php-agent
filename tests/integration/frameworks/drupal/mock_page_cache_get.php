<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Mock a drupal function that we instrument, that for the purposes
 * of this test, is valid to pass into invokeAllWith(...). This is
 * used to test attempts to overwrite special instrumentation */
namespace Drupal\page_cache\StackMiddleware {
    class PageCache {
        public function get(callable $hook, string $module) {
            $hook();
        }
    }
    echo "";
}
