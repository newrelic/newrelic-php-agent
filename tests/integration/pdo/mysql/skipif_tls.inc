<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The skipif of the tests that need the MySQL server with TLS (see
 * tests/tls). Set $tls_profile to the connection the test uses (a key of
 * $tls_profiles below) before requiring this file, and require
 * skipif_mysql.inc before it.
 *
 * A test is skipped only when it cannot run at all: PHP is below 7.4 or
 * MYSQL_TLS_HOST is not set. When the host is set but the server or a user is
 * not usable, this file exits with an error and the runner fails the test
 * with the reason, so that a broken test environment is not mistaken for a
 * pass.
 */
require_once(dirname(__FILE__) . '/../../../include/config.php');

if (version_compare(PHP_VERSION, "7.4", "<")) {
  die("skip: PHP < 7.4.0 not supported\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}

/* The SSL attributes moved to Pdo\Mysql in PHP 8.4. */
$tls_attr = class_exists('Pdo\Mysql') ? 'Pdo\Mysql::ATTR_' : 'PDO::MYSQL_ATTR_';

$tls_profiles = array(
  'required' => array('user' => 'tls_required', 'password' => 'tls_required',
                      'ssl' => array('SSL_CA' => "$MYSQL_TLS_CERTS/ca.pem")),
  'one_conn' => array('user' => 'nr_one_conn', 'password' => 'nr_one_conn'),
  'clientcert' => array('user' => 'tls_clientcert', 'password' => 'tls_clientcert',
                        'ssl' => array('SSL_CA' => "$MYSQL_TLS_CERTS/ca.pem",
                                       'SSL_KEY' => "$MYSQL_TLS_CERTS/client-key.pem",
                                       'SSL_CERT' => "$MYSQL_TLS_CERTS/client-cert.pem")),
);

if (!isset($tls_profile, $tls_profiles[$tls_profile])) {
  echo "skipif_tls.inc: unknown \$tls_profile\n";
  exit(1);
}
$tls = $tls_profiles[$tls_profile];

$tls_options = array();
if (isset($tls['ssl'])) {
  foreach ($tls['ssl'] as $name => $value) {
    $tls_options[constant($tls_attr . $name)] = $value;
  }
}
try {
  new PDO("mysql:host=$MYSQL_TLS_HOST;port=$MYSQL_TLS_PORT;dbname=$MYSQL_TLS_DB",
          $tls['user'], $tls['password'], $tls_options);
} catch (Exception $e) {
  echo "TLS MySQL server not available for user {$tls['user']}: "
       . $e->getMessage() . "\n";
  exit(1);
}
