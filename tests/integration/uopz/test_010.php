<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_get_hook.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(true)
object(Closure)#1 (1) {
  ["parameter"]=>
  array(1) {
    ["$arg"]=>
    string(10) "<required>"
  }
}
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

var_dump(uopz_get_hook(Foo::class, "method"));
