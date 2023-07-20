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
    }

    return true;
}
