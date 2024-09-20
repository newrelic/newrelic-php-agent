<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A very simple mock of laravel/framework package used to similate detection of Laravel framework
which results in php package harvest.
*/

namespace Illuminate\Foundation;

class Application
{
    const VERSION = '11.4.5';

    public function __construct()
    {
        echo "";
    }
}

// force detection on PHP 8.2+
echo "";
