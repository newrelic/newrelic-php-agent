<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This file is needed by real Composer's runtime API, and agent verifies its presence to determine if Composer is used.
This file contains phpized version of composer.json for the project and its dependencies.
*/
  return array(
    // Mocked data: root package
    'root' => array(
        'pretty_version' => 'v1.0.0',
        'version' => '1.0.0.0',
        'type' => 'project'
    ),
    // Mocked invalid package data: 
    // - package without name and version
    // - package without name but with version
    // - package with name but without version
    // Mocked valid package data: 
    // - package with name and version
    'versions' =>  array(
        array(
            'version' => '1.1.3.0',
            'type' => 'library'
        ),
        array(
            'pretty_version' => 'v2.1.3',
            'version' => '2.1.3.0',
            'type' => 'library'
        ),
        'vendor2/package2' => array(
            'version' => '3.1.5.0',
            'type' => 'library'
        ),
        'laravel/framework' => array(
            'pretty_version' => '11.4.5',
            'version' => '11.4.5',
            'type' => 'library'
        ),
    )
  );
