<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* 
 * The Monolog packages are shipped as archive in the repository
 * (tests/include/monolog2.tar.bz2, tests/include/monolog3.tar.bz2). 
 * The archive is unpacked when Monolog tests are run and `unpack_monolog` 
 * is called in the SKIPIF section. 
 * 
 * The purpose of this solution is not to clutter the repository with Monolog 
 * files and avoid using composer, while still being able to run tests in the 
 * PR builder. Just removing the Monolog archive and unpacked directory causes 
 * the tests to be skipped.
 *
 * The monolog2.tar.bz2 archive contains Monolog 2 with all its dependencies
 * as installed by composer.
 * The monolog3.tar.bz2 archive contains Monolog 3 with all its dependencies
 * as installed by composer.
 */

/*
 * `unpack_monolog` is called during the SKIPIF sections of integration tests.
 * It extracts the archive containing Monolog installation in this directory
 * and checks, if the archive contains the desired version.
 */
function unpack_monolog($version) {
    $library = 'monolog' . $version;
    $pwd = realpath(dirname(__FILE__));
    $libdir = $pwd . '/' . $library;

    if (!file_exists($libdir)) {
        exec('cd ' . $pwd . ' && tar xf ' . $library . '.tar.bz2 2>/dev/null');
    }

    return file_exists($libdir);
}

/* 
 * `require_monolog` imports the desired version of Monolog. The main purpose of
 * this function is to avoid dealing with absolute import paths in tests. In
 * this way, the archive could be unpacked in some different directory without
 * altering any test code.
 */
function require_monolog($version) {
    require_once(
        realpath(dirname(__FILE__)) .
        '/monolog' . $version . 
        '/vendor/autoload.php');
}
