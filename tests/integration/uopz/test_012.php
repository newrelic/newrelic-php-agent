<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_add_function.
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT_REGEX
bool\(true\)
bool\(true\)
string\(5\d\) "Call to private method Foo::priv\(\) from .*
string\(73\) "will not replace existing method Foo::exists, use uopz_set_return instead"
.*
*/

require __DIR__.'/load.inc';

class Foo {
	public function exists() {}
}

uopz_add_function(Foo::class, "METHOD", function(){
	return $this->priv();
});

uopz_add_function(Foo::class, "PRIV", function(){
	return true;
}, ZEND_ACC_PRIVATE);

uopz_add_function(Foo::class, "STATICFUNCTION", function(){
	return true;
}, ZEND_ACC_STATIC);

$foo = new Foo();

var_dump($foo->method());

var_dump(Foo::staticFunction());

try {
	var_dump($foo->priv());
} catch(Error $e) {
	var_dump($e->getMessage());
}

try {
	uopz_add_function(Foo::class, "exists", function() {});
} catch(Exception $e) {
	var_dump($e->getMessage());
}
