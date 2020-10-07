<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

use NewRelic\Integration\DatastoreInstance;
use NewRelic\Integration\Transaction;

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function pdo_mysql_assert_datastore_instance_is_valid(DatastoreInstance $instance)
{
  global $MYSQL_HOST, $MYSQL_DB;

  tap_assert($instance->isHost($MYSQL_HOST), 'slowsql host matches');
  tap_equal(pdo_mysql_expected_port(), $instance->portPathOrId, 'slowsql port matches');
  tap_equal($MYSQL_DB, $instance->databaseName, 'slowsql database matches');
}

function pdo_mysql_assert_datastore_instance_metric_exists(Transaction $txn)
{
  global $MYSQL_HOST;

  $host = newrelic_is_localhost($MYSQL_HOST) ? newrelic_get_hostname() : $MYSQL_HOST;
  $port = pdo_mysql_expected_port();

  $metrics = $txn->getUnscopedMetrics();
  tap_assert(isset($metrics["Datastore/instance/MySQL/$host/$port"]), 'datastore instance metric exists');
}

function pdo_mysql_expected_port()
{
  global $MYSQL_HOST, $MYSQL_PORT, $MYSQL_SOCKET;

  if ($MYSQL_HOST == 'localhost') {
    if ($MYSQL_SOCKET) {
      return $MYSQL_SOCKET;
    } else {
      return ini_get('pdo_mysql.default_socket');
    }
  }

  return (string) $MYSQL_PORT;
}
