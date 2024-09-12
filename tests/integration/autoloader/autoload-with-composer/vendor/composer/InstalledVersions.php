<?php

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
