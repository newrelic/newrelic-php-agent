<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_redefine
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
int(1)
int(2)
*/

require __DIR__.'/load.inc';

class Foo {
	const BAR = 1;
}

/*
 * We'll use constant() to avoid PHP being smart enough to fold in the actual
 * value, thereby defeating the redefinition.
 */
var_dump(constant('FOO::BAR'));

uopz_redefine(Foo::class, "BAR", 2);

var_dump(constant('FOO::BAR'));
