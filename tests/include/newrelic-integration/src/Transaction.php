<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

namespace NewRelic\Integration;

use stdClass;

/**
 * The main entry point to this library.
 *
 * Generally speaking, you'll create an instance of this object, and then call the methods on it, like so:
 *
 * <pre><code>$txn = new \NewRelic\Integration\Transaction;
 * $metrics = $txn->getScopedMetrics();
 * </code></pre>
 */
class Transaction
{
    /**
     * Returns the scoped metrics for the current transaction.
     *
     * @return Metric[] An associative array of Metric objects, keyed by metric
     *                  name.
     */
    public function getScopedMetrics()
    {
        return $this->getMetricsInternal(true);
    }

    /**
     * Returns the slow SQLs for the current transaction.
     *
     * @return SlowSQL[]
     */
    public function getSlowSQLs()
    {
        return array_map(function (array $slowsql) {
            return new SlowSQL($slowsql);
        }, newrelic_get_slowsqls());
    }

    /**
     * Returns the current transaction trace.
     *
     * @return Trace
     */
    public function getTrace()
    {
        return Trace::fromJSON(newrelic_get_trace_json());
    }

    /**
     * Returns the unscoped metrics for the current transaction.
     *
     * @return Metric[] An associative array of Metric objects, keyed by metric
     *                  name.
     */
    public function getUnscopedMetrics()
    {
        return $this->getMetricsInternal(false);
    }

    /**
     * A wrapper for `newrelic_get_metric_table()` that rekeys the returned
     * array into an associative array for easier reuse.
     *
     * @internal
     * @param boolean True for scoped metrics; false for unscoped.
     * @return Metric[] An associative array of Metric objects, keyed by metric
     *                  name.
     */
    protected function getMetricsInternal($scoped)
    {
        $metrics = array();

        // Take the metric table and rekey it by metric name.
        $raw = newrelic_get_metric_table($scoped);
        array_walk($raw, function (stdClass $metric) use (&$metrics) {
            $metric = new Metric($metric);
            $metrics[$metric->name] = $metric;
        });

        return $metrics;
    }
}
