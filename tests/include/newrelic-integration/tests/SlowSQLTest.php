<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\SlowSQL;

class SlowSQLTest extends PHPUnit_Framework_TestCase
{
    public function testConstruct()
    {
        $slowsql = new SlowSQL(array(
            'id' => 123456,
            'count' => 5,
            'min' => 1000,
            'max' => 2000,
            'total' => 7000,
            'metric' => 'Foo/Bar',
            'query' => 'SELECT foo FROM bar',
            'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
        ));

        $this->assertSame(123456, $slowsql->id);
        $this->assertSame(5, $slowsql->count);
        $this->assertSame(1000, $slowsql->min);
        $this->assertSame(2000, $slowsql->max);
        $this->assertSame(7000, $slowsql->total);
        $this->assertSame('Foo/Bar', $slowsql->metric);
        $this->assertSame('SELECT foo FROM bar', $slowsql->query);
        $this->assertSame(array(), $slowsql->params->explain_plan);
        $this->assertSame(array(), $slowsql->params->backtrace);
        $this->assertSame('localhost', $slowsql->params->host);
        $this->assertSame(5432, $slowsql->params->port_path_or_id);
        $this->assertSame('db', $slowsql->params->database_name);
    }

    /**
     * @dataProvider invalidProvider
     * @expectedException InvalidArgumentException
     */
    public function testConstructInvalid(array $slowsql)
    {
        new SlowSQL($slowsql);
    }

    /**
     * @dataProvider validProvider
     */
    public function testConstructValid(array $slowsql)
    {
        $this->assertInstanceOf('NewRelic\Integration\SlowSQL', new SlowSQL($slowsql));
    }

    public function testGetDatastoreInstance()
    {
        $input = array(
            'id' => 123456,
            'count' => 5,
            'min' => 1000,
            'max' => 2000,
            'total' => 7000,
            'metric' => 'Foo/Bar',
            'query' => 'SELECT foo FROM bar',
        );
        $slowsql = new SlowSQL($input);

        $this->assertNull($slowsql->getDatastoreInstance());

        $input['params'] = '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}';
        $slowsql = new SlowSQL($input);
        $instance = $slowsql->getDatastoreInstance();

        $this->assertSame('localhost', $instance->host);
        $this->assertSame(5432, $instance->portPathOrId);
        $this->assertSame('db', $instance->databaseName);
    }

    public function invalidProvider()
    {
        return array(
            'Empty' => array(array()),
            'Missing id' => array(array(
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing count' => array(array(
                'id' => 123456,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing min' => array(array(
                'id' => 123456,
                'count' => 5,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing max' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing total' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing metric' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
            'Missing query' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
        );
    }

    public function validProvider()
    {
        return array(
            'Without params' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
            )),
            'With params' => array(array(
                'id' => 123456,
                'count' => 5,
                'min' => 1000,
                'max' => 2000,
                'total' => 7000,
                'metric' => 'Foo/Bar',
                'query' => 'SELECT foo FROM bar',
                'params' => '{"explain_plan":[],"backtrace":[],"host":"localhost","port_path_or_id":5432,"database_name":"db"}',
            )),
        );
    }
}
