<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_set_return.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
bool(true)
int(4)
string(61) "failed to set return for Foo::nope, the method does not exist"
string(63) "failed to set return for Bar::bar, the method is defined in Foo"
*/

require __DIR__.'/load.inc';

class Foo {
	public function bar(int $arg) : int {
		return $arg;
	}
}

var_dump(uopz_set_return(Foo::class, "bar", true));

$foo = new Foo();

var_dump($foo->bar(1));

uopz_set_return(Foo::class, "bar", function(int $arg) : int {
	return $arg * 2;
}, true);

var_dump($foo->bar(2));

try {
	uopz_set_return(Foo::class, "nope", 1);
} catch(Throwable $t) {
	var_dump($t->getMessage());
}

class Bar extends Foo {}

try {
	uopz_set_return(Bar::class, "bar", null);
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
