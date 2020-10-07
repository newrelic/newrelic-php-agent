<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration;

use InvalidArgumentException;
use stdClass;

/**
 * A singular metric within a metric table.
 */
class Metric
{
    /**
     * @var integer
     */
    public $count;

    /**
     * @var float
     */
    public $exclusive;

    /**
     * @var float
     */
    public $max;

    /**
     * @var float
     */
    public $min;

    /**
     * @var float
     */
    public $name;

    /**
     * @var float
     */
    public $sumOfSquares;

    /**
     * @var float
     */
    public $total;

    /**
     * Constructs the metric from a metric object as returned by
     * newrelic_get_metric_table().
     *
     * @internal
     * @throws InvalidArgumentException
     */
    public function __construct(stdClass $metric)
    {
        if (!isset($metric->name, $metric->data)) {
            throw new InvalidArgumentException('Invalid metric object');
        }

        if (count($metric->data) < 6) {
            throw new InvalidArgumentException('Unexpected number of data elements');
        }

        $this->name = $metric->name;
        $this->count = $metric->data[0];
        $this->total = $metric->data[1];
        $this->exclusive = $metric->data[2];
        $this->max = $metric->data[3];
        $this->min = $metric->data[4];
        $this->sumOfSquares = $metric->data[5];
    }
}
