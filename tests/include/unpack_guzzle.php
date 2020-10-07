<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* 
 * The Guzzle package is shipped as archive in the repository
 * (tests/include/guzzle.tar.bz2). The archive is unpacked when Guzzle tests 
 * are run and `unpack_guzzle` is called in the SKIPIF section. 
 * 
 * The purpose of this solution is not to clutter the repository with Guzzle 
 * files and avoid using composer, while still being able to run tests in the 
 * PR builder. Just removing the Guzzle archive and unpacked directory causes 
 * the tests to be skipped.
 *
 * The guzzle.tar.bz2 archive contains Guzzle 5 and 6 installations. Other 
 * Guzzle versions can be supported by adding Guzzle installations to the 
 * archive.
 */

/*
 * `unpack_guzzle` is called during the SKIPIF sections of integration tests.
 * It extracts the archive containing Guzzle installations in this directory
 * and checks, if the archive contains the desired version.
 */
function unpack_guzzle($version) {
    $pwd = realpath(dirname(__FILE__));
    $guzzledir = $pwd . '/guzzle';

    if (!file_exists($guzzledir)) {
        exec('cd ' . $pwd . ' && tar xf guzzle.tar.bz2 2>/dev/null');
    }

    $guzzleversion = $guzzledir . '/guzzle' . $version;

    return file_exists($guzzleversion);
}

/* 
 * `require_guzzle` imports the desired version of Guzzle. The main purpose of
 * this function is to avoid dealing with absolute import paths in tests. In
 * this way, the archive could be unpacked in some different directory without
 * altering any test code.
 */
function require_guzzle($version) {
    require_once(
        realpath(dirname(__FILE__)) .
        '/guzzle' .
        '/guzzle' . $version . 
        '/vendor/autoload.php');
}
