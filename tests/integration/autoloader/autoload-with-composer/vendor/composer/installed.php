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
    // Mocked data: installed packages and their versions
    'versions' =>  array(
        'vendor1/package1' => array(
            'pretty_version' => 'v1.1.3',
            'version' => '1.1.3.0',
            'type' => 'library'
        ),
        'vendor2/package2' => array(
            'pretty_version' => '2.1.5',
            'version' => '2.1.5.0',
            'type' => 'library'
            )
    )
  );
