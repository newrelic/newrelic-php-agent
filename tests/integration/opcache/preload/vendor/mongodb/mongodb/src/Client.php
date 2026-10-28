<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * A minimal stand-in for the real mongodb/mongodb package's Aggregate
 * operation class. Depending on the PHP version, the agent (lib_mongodb.c)
 * instruments either any object's execute() call or only instances of
 * MongoDB\Operation\Executable, so this class implements that interface as the
 * real operation classes do. The agent reads the object's
 * collectionName/databaseName/namespace properties and execute()'s first
 * argument defensively - so execute() needs no real properties or
 * arguments to produce a Datastore/operation/MongoDB/aggregate segment.
 */
namespace MongoDB\Operation;

interface Executable {
    public function execute();
}

class Aggregate implements Executable {
    public function execute() {
        return null;
    }
}

