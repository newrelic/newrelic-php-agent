<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A minimal stand-in for the real mongodb/mongodb package's Aggregate
 * operation class. nr_mongodb_operation_before/after (agent/lib_mongodb.c)
 * unconditionally start and finalize a datastore segment regardless of
 * whether this object is a real MongoDB client, and read its
 * collectionName/databaseName properties and its first constructor
 * argument defensively - so execute() needs no real properties or
 * arguments to produce a Datastore/operation/MongoDB/aggregate segment.
 */
namespace MongoDB\Operation;

class Aggregate {
    public function execute() {
        return null;
    }
}

