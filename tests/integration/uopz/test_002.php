<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_get_return.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
bool(true)
bool(true)
object(Closure)#1 (0) {
}
NULL
*/

require __DIR__.'/load.inc';

class Foo {
	public function bar() {
		return false;
	}
}

var_dump(uopz_set_return(Foo::class, "bar", true));

var_dump(uopz_get_return(Foo::class, "bar"));

var_dump(uopz_set_return(Foo::class, "bar", function(){}));

var_dump(uopz_get_return(Foo::class, "bar"));

var_dump(uopz_get_return(Foo::class, "nope"));
