<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Mock enough bits of Laravel framework to test the SupervisorCommand */

namespace Laravel\Horizon\Console {
    class SupervisorCommand {
        public function handle() {
            echo "Error handle function\n";
            throw new \Error("Error occurred");
        }
    }
}
