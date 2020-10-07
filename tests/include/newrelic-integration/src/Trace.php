<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration;

use InvalidArgumentException;
use RecursiveIteratorIterator;

use NewRelic\Integration\Trace\FilterIterator;
use NewRelic\Integration\Trace\Segment;
use NewRelic\Integration\Trace\StringTable;

/**
 * Objects of this class represent an entire transaction trace, as captured by
 * the PHP agent.
 *
 * Generally speaking, this is most easily read in conjunction with
 * the newrelic-internal Trace specification
 * since these objects are created from the same JSON we send the collector.
 */
class Trace
{
    /**
     * The attributes attached to the transaction trace. If there are no
     * attributes (the normal case), this will be null.
     *
     * @var scalar[]
     */
    public $attributes;

    /**
     * The root trace segment.
     *
     * @var Segment
     */
    public $root;

    /**
     * The transaction start time. This is generally 0.
     *
     * @var integer
     */
    public $start;

    /**
     * Factory method to create a new Trace object from the internal JSON.
     *
     * @internal
     * @param string $json The trace JSON in collector format, as returned by
     *                     `newrelic_get_trace_json()`.
     * @return Trace
     * @throws InternalArgumentException
     */
    static public function fromJSON($json)
    {
        return new static(json_decode($json, true));
    }

    /**
     * Constructs a new Trace object from a decoded JSON object.
     *
     * @internal
     * @param array $input The decoded JSON (which needs to have been decoded
     *                     via `json_decode()` with the assoc parameter set to
     *                     true).
     * @throws InternalArgumentException
     */
    public function __construct(array $input)
    {
        if (count($input) < 2) {
            throw new InvalidArgumentException('Not enough elements in the input');
        }

        $trace = $this->resolveStrings(new StringTable($input[1]), $input[0]);
        if (count($trace) < 4) {
            throw new InvalidArgumentException('Not enough elements in the trace proper');
        }

        $this->start = $trace[0];
        $this->root = new Segment($trace[3]);
    }

    /**
     * Finds one or more segments, given a filter function.
     *
     * @param callable $filter A callback that will receive a Segment object,
     *                         and should return either true to retain the
     *                         segment in the final iterator or false to
     *                         discard it.
     * @return Segment[] A traversable object which, when iterated over, will
     *                   yield each segment in order.
     */
    public function findSegments($filter)
    {
        return new FilterIterator(new RecursiveIteratorIterator($this->root, RecursiveIteratorIterator::SELF_FIRST), $filter);
    }

    /**
     * Finds all segments within the transaction that match the given name.
     *
     * @param string $name The name to search for.
     * @return Segment[] A traversable object which will yield each segment
     *                   matching the given name.
     */
    public function findSegmentsByName($name)
    {
        return $this->findSegments(function (Segment $segment) use ($name) {
            return $name === $segment->name;
        });
    }

    /**
     * Finds all segments within the transaction that have datastore instance
     * metadata.
     *
     * @return Segment[] A traversable object which will yield each segment with
     *                   datastore instance metadata.
     */
    public function findSegmentsWithDatastoreInstances()
    {
        return $this->findSegments(function (Segment $segment) {
            return DatastoreInstance::hasAttributes($segment->attributes);
        });
    }

    /**
     * Resolves string references in the given trace against the string table.
     *
     * Note that this function doesn't depend on its scope, and is only
     * declared non-static to better facilitate testing and/or extension.
     *
     * @internal
     * @param StringTable $strings The trace's string table.
     * @param array       $trace   The decoded trace JSON.
     * @return array The updated trace.
     */
    protected function resolveStrings(StringTable $strings, array $trace)
    {
        $copy = $trace;
        array_walk_recursive($copy, function (&$value) use ($strings) {
            if (is_string($value)) {
                $value = $strings->resolve($value);
            }
        });

        return $copy;
    }
}
