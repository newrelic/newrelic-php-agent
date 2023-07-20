<?php
/*
 * Copyright 2023 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* 
 * Dupal core modules (bootstrap.inc, common.inc) are not embedded
 * in the repository. They are downloaded when Drupal tests are run
 * and `download_drupal`  is called in the SKIPIF section. 
 * 
 * The purpose of this solution is to avoid embedding Drupal code, while still
 * being able to run tests in the PR builder. If downloading Drupal core modules
 * fails, the test will abort with a warning.
 *
 */

/*
 * `download_drupal` is called during the SKIPIF sections of integration tests.
 * It downloads the bootstrap.inc and common.inc files from drupal project in
 * desired version.
 */
function download_drupal($version) {
    $pwd = realpath(dirname(__FILE__));
    $files = ['bootstrap.inc', 'common.inc'];
    $checksums = [
        '6' => [
            'bootstrap.inc' => '14e7070e475e020775255db84f3d3a73999418aa8b6ac51299eb34078690413b5e7aef4003c89f2538d3966b460e2dbf', 
            'common.inc' => 'b84b1e7c61cf8d28ded9c6c8b20abc52bc2962bf15ab9f08a2daa90b1c09694501a85a912a8d0e9ede842655be4c446d'],
        '7' => [
            'bootstrap.inc' => '673244b6d19dd2de6b5048f3412d62f1baf24593245175034d59f0dac305c936b0e67a1a91ba57614d17154cd82e0db5', 
            'common.inc' => 'f0b758b53f6819bc133b22987c180f947f5055d1225bf14ab2768e84a64c83283e507fcf82e6c3499e3eaf51e65340d0']
    ];
    $file_prefix = 'drupal_' . $version . '_';
    $drupal_source = 'https://raw.githubusercontent.com/drupal/drupal/';

    if (6 == $version) {
        // fd9d1859fce12f359a587e038212d30c2bd738e6 is snapshot in time of 6.x after 6.37 but before 6.38
        $drupal_commit_sha = 'fd9d1859fce12f359a587e038212d30c2bd738e6';
    } elseif (7 == $version) {
        // 728503753272f746bb988ffb30d5327f8f2df4b2 is 7.92
        $drupal_commit_sha = '728503753272f746bb988ffb30d5327f8f2df4b2';
    } else {
        /* Unsupported version */
        return false;
    }

    foreach ($files as $f) {
        $src_file_path = $drupal_source . $drupal_commit_sha . '/includes/' . $f;
        $dest_file_path = $pwd . '/' . $file_prefix . $f;
        if (!file_exists($dest_file_path)) {
            copy($src_file_path, $dest_file_path);
        }
    }

    foreach ($files as $f) {
        $file_path = $pwd . '/' . $file_prefix . $f;
        if (!file_exists($file_path)) {
            return false;
        }
        $got_checksum = hash_file('sha384', $file_path);
        $want_checksum = $checksums[$version][$f];
        if ($got_checksum != $want_checksum) {
            return false;
        }
    }

    return true;
}
