# newrelic/integration

This PHP library provides helper objects and methods to more easily parse data
provided by the PHP agent's internal testing API.

## Requirements

* PHP 5.3.2 or later.
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

This package needs to keep support for PHP 5.3, as
the PHP agent currently supports PHP 5.3. This is a bit limiting (for 
example, the filtering in the `NewRelic\Integration\Trace` class would be
*much* easier with anonymous classes), but it is necessary to maintain the
backward compatability.

### Contributing

Contributions are welcome. The most likely place that needs expansion is the
`NewRelic\Integration\Trace` class, which could use a lot more helper methods
to find specific trace segments of particular types or interesting values. You
should be able to build those on top of
`NewRelic\Integration\Trace::findSegments()` in the same style as the couple of
helpers that exist today (`findSegmentsByName()` and
`findSegmentsWithDatastoreInstances()`).

### Testing

This package is tested using
[PHPUnit 4.8](https://phpunit.de/manual/4.8/en/index.html), which is dated, 
but it is the last version of PHPUnit to support PHP
5.3. It still gets the job done. The main thing we don't get with such an old
version is easy code coverage, since it doesn't support phpdbg like later
versions of PHPUnit.

To run the tests:

```bash
composer install
./vendor/bin/phpunit
```

One gotcha: you *must* run `composer install` on the oldest version of PHP you
intend to run the test suite on, as some of PHPUnit's dependencies generate
code, and they'll generate code for newer versions of PHP if you let them.

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

### Documentation

Documentation is generated with
[Sami](https://packagist.org/packages/sami/sami).

To generate documentation, you can run Sami as follows (provided you ran
`composer install` as described in the testing section above):

```bash
./vendor/bin/sami.php update sami.php
```

This will generate documentation in the `build` directory, and metadata (which
is used to speed up subsequent builds) in `cache`.

#### Publishing

The easiest way to publish your documentation is to use the
[gh-pages](https://www.npmjs.com/package/gh-pages) tool. To use it, you'll need
[Node.js](https://nodejs.org/en/) available, along with either
[NPM](https://www.npmjs.com/) or [Yarn](https://yarnpkg.com/en/).

Getting the tool is easy enough:

```bash
yarn install
```

Actually running it isn't much harder (in this example, we're assuming that
`origin` is the name of the Git remote, and `gh-pages` is the desired branch),
provided you've already built the documentation as described above:

```bash
yarn run gh-pages -d build -b gh-pages -o origin
```
