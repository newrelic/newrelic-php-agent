<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// API documentation configuration for the Sami tool. See the README for more
// details.

use Sami\Parser\Filter\DefaultFilter;
use Sami\Reflection\MethodReflection;
use Sami\Sami;

class InternalFilter extends DefaultFilter
{
    public function acceptMethod(MethodReflection $method)
    {
        // Specifically exclude methods with @internal defined.
        if ($method->getTags('internal')) {
            return false;
        }
        return parent::acceptMethod($method);
    }
}

return new Sami(__DIR__.'/src', [
    'filter' => function () { return new InternalFilter; },
    'title' => 'newrelic/integration',
]);
