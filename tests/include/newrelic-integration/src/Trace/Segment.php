<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration\Trace;

use ArrayIterator;
use InvalidArgumentException;
use RecursiveIterator;

use NewRelic\Integration\DatastoreInstance;

/**
 * A segment within a transaction trace.
 *
 * Segment objects are also iterators: if iterated over, all child segments
 * will be yielded, in time order.
 */
class Segment extends ArrayIterator implements RecursiveIterator
{
    /**
     * The attributes attached to the segment, as a generic object with
     * properties keyed by attribute name.
     *
     * @var \stdClass
     */
    public $attributes;

    /**
     * Child segments. Generally speaking, these should be accessed by
     * iterating over the parent object, rather than directly, but the property
     * is available if required.
     *
     * @var Segment[]
     */
    public $children;

    /**
     * The segment name.
     *
     * @var string
     */
    public $name;

    /**
     * The start time of the segment, relative to the transaction start.
     *
     * @var integer
     */
    public $start;

    /**
     * The stop time of the segment, relative to the transaction start.
     *
     * @var integer
     */
    public $stop;

    /**
     * Constructs a new Segment object.
     *
     * This should only be called when constructing a Trace object, and is not
     * for general use.
     *
     * @internal
     * @param array $segment The decoded segment JSON.
     * @throws InvalidArgumentException
     */
    public function __construct(array $segment)
    {
        if (count($segment) < 5) {
            throw new InvalidArgumentException('Not enough parts in the segment');
        }

        if (!is_array($segment[3])) {
            throw new InvalidArgumentException('Attributes are not an array');
        }

        if (!is_array($segment[4])) {
            throw new InvalidArgumentException('Children are not an array');
        }

        $this->start = $segment[0];
        $this->stop = $segment[1];
        $this->name = $segment[2];
        $this->attributes = (object) $segment[3];
        $this->children = array_map(function (array $child) {
            return new Segment($child);
        }, $segment[4]);

        parent::__construct($this->children);
    }

    /**
     * Returns the datastore instance metadata attached to the segment, if any.
     *
     * @return DatastoreInstance NULL if no datastore instance metadata exists.
     */
    public function getDatastoreInstance()
    {
        if (DatastoreInstance::hasAttributes($this->attributes)) {
            return new DatastoreInstance($this->attributes);
        }

        return null;
    }

    /**
     * Returns an iterator for the segment's children.
     *
     * Implementation detail to fulfil the RecursiveIterator interface.
     *
     * @internal
     * @return RecursiveIterator
     */
    public function getChildren()
    {
        return $this->current();
    }

    /**
     * Checks if the segment has children.
     *
     * @return boolean
     */
    public function hasChildren()
    {
        return (boolean) $this->current()->count();
    }
}
