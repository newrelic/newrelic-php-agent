<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_extend
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
string(44) "the class provided (Foo) already extends Bar"
*/

require __DIR__.'/load.inc';

class Foo {}
class Bar {}

uopz_extend(Foo::class, Bar::class);

$foo = new Foo;

var_dump($foo instanceof Bar);

try {
	uopz_extend(Foo::class, Bar::class);
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
