<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_undefine
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
int(1)
int(0)
*/

require __DIR__.'/load.inc';

class Foo {
	const BAR = 1;
}

var_dump(FOO::BAR);

uopz_undefine(Foo::class, "BAR");

$reflector = new ReflectionClass(Foo::class);

var_dump(count($reflector->getConstants()));
