<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\Trace;

class TraceTest extends PHPUnit_Framework_TestCase
{
    public function testConstruct()
    {
        $trace = new Trace($this->traceData());

        $this->assertSame(0, $trace->start);
        $this->assertSame('ROOT', $trace->root->name);
    }

    /**
     * @dataProvider invalidProvider
     * @expectedException InvalidArgumentException
     */
    public function testConstructInvalid(array $input)
    {
        new Trace($input);
    }

    public function testFindSegmentsByName()
    {
        $trace = new Trace($this->traceData());
        $segments = iterator_to_array($trace->findSegmentsByName('foo'));

        $this->assertCount(0, $segments);

        $segments = iterator_to_array($trace->findSegmentsByName('older child'));

        $this->assertCount(1, $segments);
        $this->assertInstanceOf('NewRelic\Integration\Trace\Segment', $segments[0]);
        $this->assertSame('older child', $segments[0]->name);
    }

    public function testFindSegmentsWithDatastoreInstances()
    {
        $trace = new Trace($this->traceData());
        $segments = iterator_to_array($trace->findSegmentsWithDatastoreInstances());

        $this->assertCount(1, $segments);
        $this->assertInstanceOf('NewRelic\Integration\Trace\Segment', $segments[0]);
        $this->assertSame('grandchild', $segments[0]->name);
    }

    public function invalidProvider()
    {
        return array(
            'Empty input' => array(array()),
            'Not enough input elements' => array(array(
                array(),
            )),
            'Empty trace' => array(array(
                array(),
                array(),
            )),
            'Not enough trace elements' => array(array(
                array(0, null, null),
                array(),
            )),
        );
    }

    public function traceData()
    {
        return array(
            array(
                0,
                array(),
                array(),
                array(
                    0,
                    10,
                    '`0',
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
                                    array(
                                        'host' => 'localhost',
                                        'port_path_or_id' => 5432,
                                        'database_name' => 'db',
                                    ),
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
                ),
            ),
            array('ROOT'),
        );
    }
}
