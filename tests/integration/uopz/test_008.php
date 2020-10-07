<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_set_static
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
array(1) {
  ["vars"]=>
  array(6) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
    [3]=>
    int(4)
    [4]=>
    int(5)
    [5]=>
    int(6)
  }
}
array(1) {
  ["vars"]=>
  array(1) {
    [0]=>
    int(6)
  }
}
*/

require __DIR__.'/load.inc';

class Foo {
	public function method() {
		static $vars = [
			1,2,3,4,5];

		$vars[] = 6;
	}
}

$foo = new Foo();

$foo->method();

var_dump(uopz_get_static(Foo::class, "method"));

uopz_set_static(Foo::class, "method", [
	"vars" => []
]);

$foo->method();

var_dump(uopz_get_static(Foo::class, "method"));
