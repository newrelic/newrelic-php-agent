<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_unset_mock.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
string(3) "Bar"
NULL
*/

require __DIR__.'/load.inc';

class Bar {}

uopz_set_mock(Foo::class, Bar::class);

var_dump(uopz_get_mock(Foo::class));

uopz_unset_mock(Foo::class);

var_dump(uopz_get_mock(Foo::class));
