<?php
/*
 * Copyright 2025 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should correctly instrument attributes.
*/

/*SKIPIF
<?php
if (version_compare(PHP_VERSION, '8.0', '<')) {
  die("skip: attributes are not available for PHP versions < 8.0");
}
*/

/*INI
newrelic.code_level_metrics.enabled = true
*/

/*EXPECT_METRICS_EXIST
Custom/ExampleAttribute::getInfo, 2
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
      "name": "Custom\/ExampleAttribute::getInfo",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {},
    {
      "code.lineno": "??",
      "code.namespace": "ExampleAttribute",
      "code.filepath": "__FILE__",
      "code.function": "getInfo"
    }
  ],
  [
    {
      "category": "generic",
      "type": "Span",
      "guid": "??",
      "traceId": "??",
      "transactionId": "??",
      "name": "Custom\/ExampleAttribute::getInfo",
      "timestamp": "??",
      "duration": "??",
      "priority": "??",
      "sampled": true,
      "parentId": "??"
    },
    {},
    {
      "code.lineno": "??",
      "code.namespace": "ExampleAttribute",
      "code.filepath": "__FILE__",
      "code.function": "getInfo"
    }
  ]
]
*/

/*EXPECT_TRACED_ERRORS null*/

/*EXPECT
Class Attribute Info: This is a sample attribute
Method Attribute Info: This is a method attribute
*/

#[Attribute]
class ExampleAttribute
{
    public function __construct(public string $info) {}
    public function getInfo(): string
    {
        return $this->info;
    }
}

#[ExampleAttribute("This is a sample attribute")]
class SampleClass
{
    #[ExampleAttribute("This is a method attribute")]
    public function sampleMethod() {}
}

newrelic_add_custom_tracer("ExampleAttribute::getInfo");

// Reflect the class
$reflectionClass = new ReflectionClass(SampleClass::class);

// Get class attributes
$classAttributes = $reflectionClass->getAttributes(ExampleAttribute::class);

foreach ($classAttributes as $attribute) {
    $instance = $attribute->newInstance();
    echo "Class Attribute Info: " . $instance->getInfo() . "\n";
}

// Reflect the method
$reflectionMethod = $reflectionClass->getMethod('sampleMethod');

// Get method attributes
$methodAttributes = $reflectionMethod->getAttributes(ExampleAttribute::class);

foreach ($methodAttributes as $attribute) {
    $instance = $attribute->newInstance();
    echo "Method Attribute Info: " . $instance->getInfo() . "\n";
}
