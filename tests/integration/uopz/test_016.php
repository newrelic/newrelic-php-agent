<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
uopz_flags
*/

/*SKIPIF
<?php include("skipif.inc") ?>
*/

/*EXPECT
bool(false)
bool(true)
*/

require __DIR__.'/load.inc';

class Foo {
	public function method() {}
}

var_dump((bool) (uopz_flags(Foo::class, "method", ZEND_ACC_PRIVATE) & ZEND_ACC_PRIVATE));
var_dump((bool) (uopz_flags(Foo::class, "method", PHP_INT_MAX) & ZEND_ACC_PRIVATE));
