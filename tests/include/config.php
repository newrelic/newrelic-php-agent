<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/* Basic null-coalescing function.  */
function isset_or($check, $alternate = NULL)
{
  $env_value = getenv($check);
  return (false !== $env_value) ? $env_value : $alternate;
}

$MYSQL_USER   = isset_or('MYSQL_USER', 'root');
$MYSQL_PASSWD = isset_or('MYSQL_PASSWD', 'root');
$MYSQL_DB     = isset_or('MYSQL_DB', 'information_schema');
$MYSQL_HOST   = isset_or('MYSQL_HOST', 'localhost');
$MYSQL_PORT   = isset_or('MYSQL_PORT', 3306);
$MYSQL_SOCKET = isset_or('MYSQL_SOCKET', '');

if ("" != $MYSQL_SOCKET) {
  $MYSQL_SERVER = $MYSQL_HOST . ":" . $MYSQL_SOCKET;
} else {
  $MYSQL_SERVER = $MYSQL_HOST . ":" . $MYSQL_PORT;
}

$MEMCACHE_HOST = isset_or('MEMCACHE_HOST', '127.0.0.1');
$MEMCACHE_PORT = isset_or('MEMCACHE_PORT', '11211');

$REDIS_HOST = isset_or("REDIS_HOST", "127.0.0.1");
$REDIS_PORT = isset_or("REDIS_PORT", 6379);

$PDO_MYSQL_DSN = 'mysql:';
$PDO_MYSQL_DSN .= 'host=' . $MYSQL_HOST . ';';
$PDO_MYSQL_DSN .= 'dbname=' . $MYSQL_DB . ';';

if ("" != $MYSQL_SOCKET) {
  $PDO_MYSQL_DSN .= 'unix_socket=' . $MYSQL_SOCKET . ';';
} else if ("" != $MYSQL_PORT) {
  $PDO_MYSQL_DSN .= 'port=' . $MYSQL_PORT . ';';
}

$EXTERNAL_HOST = getenv('EXTERNAL_HOST');
$EXTERNAL_TRACING_URL = $EXTERNAL_HOST . "/cat";

function make_dt_enabled_param()
{
    $value = "false";
    if (ini_get("newrelic.distributed_tracing_enabled"))
        $value = "true";

    return "dt_enabled=" . $value;
}

function make_cat_enabled_param()
{
    $value = "false";
    if (ini_get("newrelic.cross_application_tracer.enabled"))
        $value = "true";

    return "cat_enabled=" . $value;
}

function make_tracing_url($file)
{
    global $EXTERNAL_TRACING_URL;

    return $EXTERNAL_TRACING_URL . '?file=' . $file .
            '&' . make_dt_enabled_param() . '&' . make_cat_enabled_param();
}

$PG_USER       = isset_or('PG_USER', 'postgres');
$PG_PW         = isset_or('PG_PW', 'root');
$PG_HOST       = isset_or('PG_HOST', 'localhost');
$PG_PORT       = isset_or('PG_PORT', '5433');
$PG_CONNECTION = "host=$PG_HOST port=$PG_PORT user=$PG_USER password=$PG_PW connect_timeout=1";

// Header used to track whether or not our CAT instrumentation interferes with
// other existing headers.
define('CUSTOMER_HEADER', 'Customer-Header');

// CAT Headers
define('X_NEWRELIC_ID',          'X-NewRelic-ID');
define('X_NEWRELIC_TRANSACTION', 'X-NewRelic-Transaction');
define('X_NEWRELIC_APP_DATA',    'X-NewRelic-App-Data');

// DT Headers
define('DT_NEWRELIC',            'newrelic');
define('DT_TRACEPARENT',         'traceparent');
define('DT_TRACESTATE',          'tracestate');

// Synthetics Headers
define('X_NEWRELIC_SYNTHETICS', 'X-NewRelic-Synthetics');
