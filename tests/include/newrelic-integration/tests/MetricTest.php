<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\Metric;

class MetricTest extends PHPUnit_Framework_TestCase
{
    public function testConstruct()
    {
        $metric = new Metric((object) array(
            'name' => 'Foo/Bar',
            'data' => array(
                5,
                4,
                3,
                2,
                1,
                0.5,
            ),
        ));

        $this->assertSame('Foo/Bar', $metric->name);
        $this->assertSame(5, $metric->count);
        $this->assertSame(4, $metric->total);
        $this->assertSame(3, $metric->exclusive);
        $this->assertSame(2, $metric->max);
        $this->assertSame(1, $metric->min);
        $this->assertSame(0.5, $metric->sumOfSquares);
    }

    /**
     * @dataProvider invalidMetrics
     * @expectedException InvalidArgumentException
     */
    public function testConstructInvalid($metric)
    {
        new Metric($metric);
    }

    public function invalidMetrics()
    {
        return array(
            'Empty object' => array((object) array()),
            'Missing name' => array((object) array('data' => array())),
            'Missing data' => array((object) array('name' => 'Foo/Bar')),
            'Missing data elements' => array((object) array(
                'name' => 'Foo/Bar',
                'data' => array(0, 0, 0, 0, 0),
            )),
        );
    }
}
