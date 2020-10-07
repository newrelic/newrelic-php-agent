<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\Trace\StringTable;

class StringTableTest extends PHPUnit_Framework_TestCase
{
    /**
     * @expectedException OutOfBoundsException
     */
    public function testGetOutOfBounds()
    {
        $table = new StringTable($this->sampleStringTable());
        $table->get(26);
    }

    /**
     * @dataProvider resolveProvider
     */
    public function testResolve($expected, $input)
    {
        $table = new StringTable($this->sampleStringTable());
        $this->assertSame($expected, $table->resolve($input));
    }

    public function resolveProvider()
    {
        return array(
            'Normal reference' => array('a', '`0'),
            'Double digit reference' => array('z', '`25'),
            'Empty string' => array('', ''),
            'Non-reference string' => array('foo', 'foo'),
            'Leading junk' => array(' `0', ' `0'),
            'Trailing junk' => array('`0 ', '`0 '),
        );
    }

    public function sampleStringTable()
    {
        return array(
            'a',
            'b',
            'c',
            'd',
            'e',
            'f',
            'g',
            'h',
            'i',
            'j',
            'k',
            'l',
            'm',
            'n',
            'o',
            'p',
            'q',
            'r',
            's',
            't',
            'u',
            'v',
            'w',
            'x',
            'y',
            'z',
        );
    }
}
