<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\Trace\Segment;

class SegmentTest extends PHPUnit_Framework_TestCase
{
    public function testConstruct()
    {
        $input = array(
            0,
            1,
            'ROOT',
            array(),
            array(),
        );
        $segment = new Segment($input);

        $this->assertSame(0, $segment->start);
        $this->assertSame(1, $segment->stop);
        $this->assertSame('ROOT', $segment->name);
        $this->assertEquals((object) array(), $segment->attributes);
        $this->assertSame(array(), $segment->children);

        $input[3] = array('foo' => 'bar');
        $segment = new Segment($input);
        $this->assertInstanceOf('stdClass', $segment->attributes);
        $this->assertSame('bar', $segment->attributes->foo);

        $input[4] = array($input);
        $segment = new Segment($input);
        $this->assertCount(1, $segment->children);
        $this->assertInstanceOf(get_class($segment), $segment->children[0]);
    }

    /**
     * @dataProvider invalidProvider
     * @expectedException InvalidArgumentException
     */
    public function testConstructInvalid(array $input)
    {
        new Segment($input);
    }

    public function testGetDatastoreInstance()
    {
        $input = array(
            0,
            1,
            'ROOT',
            array(),
            array(),
        );
        $segment = new Segment($input);

        $this->assertNull($segment->getDatastoreInstance());

        $input[3] = array(
            'host' => 'localhost',
            'port_path_or_id' => 5432,
            'database_name' => 'db',
        );
        $segment = new Segment($input);
        $instance = $segment->getDatastoreInstance();

        $this->assertInstanceOf('NewRelic\Integration\DatastoreInstance', $instance);
        $this->assertSame('localhost', $instance->host);
        $this->assertSame(5432, $instance->portPathOrId);
        $this->assertSame('db', $instance->databaseName);
    }

    public function testIteration()
    {
        $input = array(
            0,
            10,
            'ROOT',
            array(),
            array(
                array(
                    0,
                    9,
                    'older child',
                    array(),
                    array(
                        array(
                            1,
                            8,
                            'grandchild',
                            array(),
                            array(),
                        ),
                    ),
                ),
                array(
                    9,
                    10,
                    'younger child',
                    array(),
                    array()
                ),
            ),
        );
        $segment = new Segment($input);
        $iterator = new RecursiveIteratorIterator($segment, RecursiveIteratorIterator::SELF_FIRST);

        $expected = array('older child', 'grandchild', 'younger child');
        $i = 0;
        foreach ($iterator as $v) {
            $this->assertInstanceOf(get_class($segment), $v);
            $this->assertSame($expected[$i++], $v->name);
        }
    }

    public function testIterationEmpty()
    {
        $input = array(
            0,
            1,
            'ROOT',
            array(),
            array(),
        );
        $segment = new Segment($input);
        $iterator = new RecursiveIteratorIterator($segment);

        $this->assertCount(0, iterator_to_array($iterator));
    }

    public function invalidProvider()
    {
        return array(
            'Empty' => array(array()),
            'Not enough segments' => array(array(
                0,
                1,
                'ROOT',
                array(),
            )),
            'Invalid attributes' => array(array(
                0,
                1,
                'ROOT',
                null,
                array(),
            )),
            'Invalid children' => array(array(
                0,
                1,
                'ROOT',
                array(),
                null,
            )),
        );
    }
}
