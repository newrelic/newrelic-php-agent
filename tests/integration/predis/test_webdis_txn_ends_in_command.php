<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
A transaction that a command ends and starts again while
WebdisConnection::executeCommand() runs SHALL NOT break the agent, and the
command SHALL NOT be recorded in either transaction.

The use of freed memory this guards against is only detected under valgrind.
*/

/*SKIPIF
<?php
require(realpath(dirname(__FILE__)) . '/predis.inc');
*/

/*INI
error_reporting = E_ALL & ~E_DEPRECATED & ~E_WARNING
*/

/*EXPECT
transaction restarted
done
*/

/*EXPECT_METRICS_DONT_EXIST
Datastore/operation/Redis/get
*/

require_once(realpath(dirname(__FILE__)) . '/predis.inc');

class EndsTransactionCommand extends Predis\Command\Command {
  private $ended = false;

  public function getId() {
    if ($this->ended) {
      return 'GET';
    }
    foreach (debug_backtrace(DEBUG_BACKTRACE_IGNORE_ARGS) as $frame) {
      if ('executeCommand' === $frame['function']) {
        $this->ended = true;
        newrelic_end_transaction();
        newrelic_start_transaction(ini_get("newrelic.appname"));
        echo "transaction restarted\n";
        break;
      }
    }
    return 'GET';
  }
}

/*
 * The connection is created without its constructor, which needs the phpiredis
 * extension, and is never opened: executeCommand() only needs the command and a
 * curl handle. The function its destructor calls is defined here.
 */
eval('namespace Predis\\Connection; function phpiredis_reader_destroy($reader) {}');

$class = new ReflectionClass('Predis\Connection\WebdisConnection');
$connection = $class->newInstanceWithoutConstructor();
$resource = $class->getProperty('resource');
$resource->setAccessible(true);
$resource->setValue($connection, curl_init());

try {
  $connection->executeCommand(new EndsTransactionCommand(array("nr_key")));
} catch (Throwable $e) {
}
echo "done\n";
