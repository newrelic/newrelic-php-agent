<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_unset_return
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
*/

require __DIR__.'/load.inc';

class Foo {
	public function bar() {
		return false;
	}
}

var_dump(uopz_set_return(Foo::class, "bar", true));

$foo = new Foo();

var_dump($foo->bar());

var_dump(uopz_unset_return(Foo::class, "bar"));

var_dump($foo->bar());

var_dump(uopz_unset_return(Foo::class, "nope"));
