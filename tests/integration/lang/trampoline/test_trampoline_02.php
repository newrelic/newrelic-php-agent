<?php
/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that trampoline functions not blow up.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, '8.0', '<')) {
  die("skip: custom instrumenting methods of anonymous classes is not available for PHP versions < 8.0");
}
*/

/*INI
newrelic.code_level_metrics.enabled = true
*/

/*EXPECT_METRICS_EXIST
Custom/class@anonymous::execute, 2
*/

/*EXPECT_SPAN_EVENTS_LIKE
[
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/class@anonymous::execute",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {},
    {
      "code.lineno": "??",
      "code.namespace": "class@anonymous",
      "code.filepath": "__FILE__",
      "code.function": "execute"
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/class@anonymous::execute",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {},
    {
      "code.lineno": "??",
      "code.namespace": "class@anonymous",
      "code.filepath": "__FILE__",
      "code.function": "execute"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS null*/

$anon = new class() {
  function execute()
  {
    new ReflectionClass('Wrapper');
  }
};

$anon2 = new class() {
  function execute()
  {
    new ReflectionClass('Wrapper');
  }
};

$klass = new ReflectionClass($anon);
$method = $klass->getMethod('execute');
$klass_name = $klass->getName();
$method_name = $method->getName();

newrelic_add_custom_tracer("$klass_name::$method_name");

(new Wrapper($anon))->execute(
  (new Wrapper($anon))->execute()
);

class Wrapper
{
  protected $wrapped;

  function __construct($wrapped)
  {
    $this->wrapped = $wrapped;
  }

  public function __call($method, $arguments)
  {
    return call_user_func([$this->wrapped, $method]);
  }
}
