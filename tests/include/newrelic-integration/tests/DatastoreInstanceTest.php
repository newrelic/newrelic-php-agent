<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\DatastoreInstance;

class DatastoreInstanceTest extends PHPUnit_Framework_TestCase
{
    public function testConstruct()
    {
        $instance = new DatastoreInstance((object) array(
            'host' => 'localhost',
            'port_path_or_id' => 1234,
            'database_name' => 'db',
        ));

        $this->assertSame('localhost', $instance->host);
        $this->assertSame(1234, $instance->portPathOrId);
        $this->assertSame('db', $instance->databaseName);
    }

    /**
     * @dataProvider invalidInputs
     * @expectedException InvalidArgumentException
     */
    public function testConstructInvalid(stdClass $input)
    {
        new DatastoreInstance($input);
    }

    /**
     * @dataProvider validInputs
     */
    public function testConstructValid(stdClass $input)
    {
        $this->assertInstanceOf('NewRelic\Integration\DatastoreInstance', new DatastoreInstance($input));
    }

    /**
     * @dataProvider allInputs
     */
    public function testHasAttributes(stdClass $input, $expected)
    {
        $this->assertSame($expected, DatastoreInstance::hasAttributes($input));
    }

    /**
     * @dataProvider hostInputs
     * @requires function newrelic_is_recording
     */
    public function testIsHost($expected, $instance, $actual)
    {
        if (!newrelic_is_recording()) {
            $this->markTestSkipped('PHP agent is not recording');
            return;
        }

        $instance = new DatastoreInstance((object) array(
            'host' => $instance,
            'port_path_or_id' => 1234,
            'database_name' => 'db',
        ));

        $this->assertSame($expected, $instance->isHost($actual));
    }

    /**
     * @requires function newrelic_is_recording
     */
    public function testIsLocalhost()
    {
        if (!newrelic_is_recording()) {
            $this->markTestSkipped('PHP agent is not recording');
            return;
        }

        $instance = new DatastoreInstance((object) array(
            'host' => 'localhost',
            'port_path_or_id' => 1234,
            'database_name' => 'db',
        ));

        $this->assertTrue($instance->isLocalhost());

        $instance = new DatastoreInstance((object) array(
            'host' => newrelic_get_hostname(),
            'port_path_or_id' => 1234,
            'database_name' => 'db',
        ));

        $this->assertTrue($instance->isLocalhost());

        $instance = new DatastoreInstance((object) array(
            'host' => 'foo.bar',
            'port_path_or_id' => 1234,
            'database_name' => 'db',
        ));

        $this->assertFalse($instance->isLocalhost());
    }

    public function allInputs()
    {
        return array_merge(
            array_map(function ($input) {
                return array($input[0], false);
            }, $this->invalidInputs()),
            array_map(function ($input) {
                return array($input[0], true);
            }, $this->validInputs())
        );
    }

    public function hostInputs()
    {
        if (!function_exists('newrelic_get_hostname')) {
            return array();
        }

        return array(
            'both localhost' => array(true, newrelic_get_hostname(), 'localhost'),
            'both actual host name' => array(true, newrelic_get_hostname(), newrelic_get_hostname()),
            'both is_localhost' => array(true, newrelic_get_hostname(), '127.0.0.1'),
            'neither localhost' => array(true, 'other.host', 'other.host'),
            'one localhost' => array(false, newrelic_get_hostname(), 'other.host'),
            'not a match' => array(false, 'other.host', 'another.host'),
        );
    }

    public function invalidInputs()
    {
        return array(
            'Empty' => array((object) array()),
            'Missing host' => array((object) array(
                'port_path_or_id' => 1234,
                'database_name' => 'db',
            )),
            'Missing port_path_or_id' => array((object) array(
                'host' => 'localhost',
                'database_name' => 'db',
            )),
            'Missing database' => array((object) array(
                'host' => 'localhost',
                'port_path_or_id' => 1234,
            )),
        );
    }

    public function validInputs()
    {
        return array(
            'Localhost' => array((object) array(
                'host' => 'localhost',
                'port_path_or_id' => '/tmp/foo.sock',
                'database_name' => 'db',
            )),
            'Other host' => array((object) array(
                'host' => 'other.host',
                'port_path_or_id' => 1234,
                'database_name' => 'db',
            )),
        );
    }
}
