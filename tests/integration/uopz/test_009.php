<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_set_hook
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
bool(true)
object(Foo)#2 (0) {
}
string(64) "failed to set hook for Bar::method, the method is defined in Foo"

*/

require __DIR__.'/load.inc';

class Foo {
	public function method($arg) {

	}
}

var_dump(uopz_set_hook(Foo::class, "method", function($arg){
	var_dump($arg);
	var_dump($this);
}));

$foo = new Foo();

$foo->method(true);

class Bar extends Foo {}

try {
	uopz_set_hook(Bar::class, "method", function(){});
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
