# newrelic/integration

This PHP library provides helper objects and methods to more easily parse data
provided by the PHP agent's internal testing API.

## Requirements

* PHP 7.2 or later.
* The New Relic PHP agent with integration test helper functions available.

## Usage


### Actual usage

For example, to get every trace node that has a datastore instance:

```php
<?php

use NewRelic\Integration\Transaction;

// ...

$txn = new NewRelic\Integration\Transaction;
$trace = $txn->getTrace();
foreach ($trace->findSegmentsWithDatastoreInstances() as $segment) {
    // do stuff with the Segment object
}
```

To get the unscoped metrics for the transaction as an associative array:

```php
<?php

use NewRelic\Integration\Transaction;

// ...

$txn = new NewRelic\Integration\Transaction;
$metrics = $txn->getUnscopedMetrics();

// Is the External/all metric set?
if (isset($metrics['External/all'])) {
    // do stuff
}

// Or what's the count of the Datastore/operation/SQLite/insert metric?
if ($metrics['Datastore/operation/SQLite/insert']->count < 4) {
    // error?
}
```

## Development

### Contributing

Contributions are welcome. The most likely place that needs expansion is the
`NewRelic\Integration\Trace` class, which could use a lot more helper methods
to find specific trace segments of particular types or interesting values. You
should be able to build those on top of
`NewRelic\Integration\Trace::findSegments()` in the same style as the couple of
helpers that exist today (`findSegmentsByName()` and
`findSegmentsWithDatastoreInstances()`).

### Testing

This package is tested using [PHPUnit 8.5](https://phpunit.de/manual/8.5/en/index.html).

To run the tests:

```bash
composer install
./vendor/bin/phpunit
```

If you don't have the PHP agent available in your default PHP, the tests will
still run, but a few tests will be skipped. You can provide a path to PHP
before PHPUnit (and options, if needed); for example:

```bash
/usr/bin/php ./vendor/bin/phpunit
/usr/bin/php -d extension=../php_agent/newrelic.so ./vendor/bin/phpunit
```

You may, of course, also need to provide additional configuration settings if
required settings like `newrelic.license` aren't set by default within your
PHP. Effectively, you need to start a transaction just like any other PHP
transaction.
