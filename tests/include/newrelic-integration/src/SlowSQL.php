<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration;

use InvalidArgumentException;

/**
 * A singular SlowSQL within a transaction.
 */
class SlowSQL
{
    /**
     * @var integer
     */
    public $count;

    /**
     * @var string
     */
    public $id;

    /**
     * @var float
     */
    public $max;

    /**
     * @var string
     */
    public $metric;

    /**
     * @var float
     */
    public $min;

    /**
     * @var stdClass
     */
    public $params;

    /**
     * @var string
     */
    public $query;

    /**
     * @var float
     */
    public $total;

    /**
     * Constructs a new object from a slow SQL in the format returned by
     * newrelic_get_slowsqls().
     *
     * @internal
     * @throws InvalidArgumentException
     */
    public function __construct(array $slowsql)
    {
        foreach (array('id', 'count', 'min', 'max', 'total', 'metric', 'query') as $name) {
            if (!isset($slowsql[$name])) {
                throw new InvalidArgumentException("Missing element $name");
            }
            $this->$name = $slowsql[$name];
        }

        $this->params = isset($slowsql['params']) ? json_decode($slowsql['params']) : null;
    }

    /**
     * Returns the datastore instance for the slow SQL, if one exists.
     *
     * @return DatastoreInstance NULL is returned if the object doesn't have a
     *                           datastore instance.
     */
    public function getDatastoreInstance()
    {
        if ($this->params && DatastoreInstance::hasAttributes($this->params)) {
            return new DatastoreInstance($this->params);
        }

        return null;
    }
}
