<?php
/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The skipif of the tests that need the MySQL server with TLS (see
 * tests/tls). Set $tls_profile to the connection the test uses (a key of
 * $tls_profiles below) before requiring this file, and require skipif.inc
 * before it.
 *
 * A test is skipped only when it cannot run at all: PHP is below 7.4 or
 * MYSQL_TLS_HOST is not set. When the host is set but the server or a user is
 * not usable, this file exits with an error and the runner fails the test
 * with the reason, so that a broken test environment is not mistaken for a
 * pass.
 */
require_once(dirname(__FILE__) . '/../../include/config.php');

if (version_compare(PHP_VERSION, "7.4", "<")) {
  die("skip: PHP < 7.4.0 not supported\n");
}
if ('' == $MYSQL_TLS_HOST) {
  die("skip: MYSQL_TLS_HOST is not set\n");
}
mysqli_report(MYSQLI_REPORT_OFF);

$tls_profiles = array(
  'optional' => array('user' => 'tls_optional', 'password' => 'tls_optional'),
  'required' => array('user' => 'tls_required', 'password' => 'tls_required',
                      'flags' => MYSQLI_CLIENT_SSL),
  'nopass' => array('user' => 'nr_nopass', 'password' => null),
  'one_conn' => array('user' => 'nr_one_conn', 'password' => 'nr_one_conn'),
  'clientcert' => array('user' => 'tls_clientcert', 'password' => 'tls_clientcert',
                        'flags' => MYSQLI_CLIENT_SSL,
                        'ssl' => array("$MYSQL_TLS_CERTS/client-key.pem",
                                       "$MYSQL_TLS_CERTS/client-cert.pem",
                                       "$MYSQL_TLS_CERTS/ca.pem")),
  'verify_ca' => array('user' => 'tls_required', 'password' => 'tls_required',
                       'flags' => MYSQLI_CLIENT_SSL,
                       'ssl' => array(null, null, "$MYSQL_TLS_CERTS/ca.pem"),
                       'verify_server_cert' => true),
  'view_only' => array('user' => 'nr_view_only', 'password' => 'nr_view_only',
                       'query' => 'SELECT id FROM nr_view_priv_v'),
);

if (!isset($tls_profile, $tls_profiles[$tls_profile])) {
  echo "skipif_tls.inc: unknown \$tls_profile\n";
  exit(1);
}
$tls = $tls_profiles[$tls_profile];

$link = mysqli_init();
if (isset($tls['ssl'])) {
  mysqli_ssl_set($link, $tls['ssl'][0], $tls['ssl'][1], $tls['ssl'][2], null, null);
}
if (!empty($tls['verify_server_cert'])) {
  mysqli_options($link, MYSQLI_OPT_SSL_VERIFY_SERVER_CERT, 1);
}
if (!@mysqli_real_connect($link, $MYSQL_TLS_HOST, $tls['user'], $tls['password'],
                          $MYSQL_TLS_DB, (int)$MYSQL_TLS_PORT, null,
                          isset($tls['flags']) ? $tls['flags'] : 0)) {
  echo "TLS MySQL server not available for user {$tls['user']}: "
       . mysqli_connect_error() . "\n";
  exit(1);
}
if (isset($tls['query']) && !@mysqli_query($link, $tls['query'])) {
  echo "user {$tls['user']} cannot run {$tls['query']}: " . mysqli_error($link) . "\n";
  exit(1);
}
mysqli_close($link);
