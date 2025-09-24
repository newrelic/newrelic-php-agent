<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Mock enough bits of Laravel framework to test the HorizonCommand */

namespace Laravel\Horizon\Console {
    class HorizonCommand {
        public function handle() {
            echo "handle function\n";
            throw new \Exception("Error occurred");
        }
    }
}
