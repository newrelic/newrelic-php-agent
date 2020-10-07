<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration\Trace;

use FilterIterator as BaseFilterIterator;
use Iterator;

/**
 * An extended version of the default FilterIterator that ships with PHP that
 * allows the use of a filter callback without requiring the user to extend the
 * FilterIterator class directly.
 */
class FilterIterator extends BaseFilterIterator
{
    /**
     * The filter callback.
     *
     * @var callable
     */
    protected $filter;

    /**
     * Constructs a new filter iterator.
     *
     * @param Iterator $it     The iterator that is to be filtered.
     * @param callable $filter The callback that, given an element within the
     *                         iterator, either returns true to retain it or
     *                         false to discard it.
     */
    public function __construct(Iterator $it, $filter)
    {
        parent::__construct($it);
        $this->filter = $filter;
    }

    /**
     * The `accept()` implementation that delegates to the filter callback.
     *
     * @return boolean
     */
    public function accept()
    {
        $filter = $this->filter;
        return $filter($this->current());
    }
}
