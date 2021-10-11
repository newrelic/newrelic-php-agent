<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test function uopz_implement.
*/

/*SKIPIF
<?php
include("skipif.inc");
if (extension_loaded('uopz') && !function_exists("uopz_extend")) {
  die("skip: needs uopz < 7.1");
}
*/

/*EXPECT
bool(true)
string(54) "the class provided (Bar) already has the interface Foo"
*/

require __DIR__.'/load.inc';

interface Foo {}
class Bar {}

uopz_implement(Bar::class, Foo::class);

$bar = new Bar;

var_dump($bar instanceof Foo);

try {
	uopz_implement(Bar::class, Foo::class);
} catch (Throwable $t) {
	var_dump($t->getMessage());
}
