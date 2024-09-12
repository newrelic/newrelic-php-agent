<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Mock of 'composer show' used by integration tests to generate list of packages.
*/


include "autoload-with-composer/vendor/composer/InstalledVersions.php";
Composer\InstalledVersions::show();
