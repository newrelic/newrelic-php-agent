<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This file is the core of real Composer's runtime API, and agent verifies its presence to determine if Composer is used.
It contains \Composer\InstalledVersions class with methods used by the agent to get the installed packages and their versions.
*/

namespace Composer;
class InstalledVersions
{
    private static $installed = [
        'vendor1/package1' => '1.1.3',
        'vendor2/package2' => '2.1.5'
    ];

    public static function getVersion(string $packageName)
    {
        return self::$installed[$packageName];
    }

    public static function getInstalledPackages()
    {
        // Return the package names
        return array_keys(self::$installed);
    }
}
