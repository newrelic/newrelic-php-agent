<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that CAT works with file_get_contents when $file_get_contents is a compiled
variable.
*/

/*SKIPIF
<?php
if (!isset($_ENV["ACCOUNT_supportability"]) || !isset($_ENV["APP_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
*/

/*EXPECT
tracing endpoint reached
1
tracing endpoint reached
1
tracing endpoint reached
1
tracing endpoint reached
1
tracing endpoint reached
1
*/

/*EXPECT_RESPONSE_HEADERS
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
X-NewRelic-App-Data=??
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/all"},                                       [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                                  [5, "??", "??", "??", "??", "??"]],
    [{"name":"External/127.0.0.1/all"},                             [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalApp/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/all"}, [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing"}, [5, "??", "??", "??", "??", "??"]],
    [{"name":"ExternalTransaction/127.0.0.1/ENV[ACCOUNT_supportability]#ENV[APP_supportability]/WebTransaction/Custom/tracing",
      "scope":"OtherTransaction/php__FILE__"},                      [5, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                               [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},              [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Forwarding/PHP/enabled"},      [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Metrics/PHP/enabled"},         [1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/LocalDecorating/PHP/disabled"},[1, "??", "??", "??", "??", "??"]],
    [{"name":"Supportability/Logging/Labels/PHP/disabled"},         [1, "??", "??", "??", "??", "??"]]
  ]
]
*/




require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');

function f() {
  $url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../../include/tracing_endpoint.php');

  /*
   * Seeking (offset) is not supported with remote files, so testing if this
   * parameter is maintained is not important. Similarly, I don't think
   * use_include_path has any effect on remote files.
   */

  /* only URL */
  echo file_get_contents ($url);
  echo is_array($http_response_header)."\n";

  /* no context */
  echo file_get_contents ($url, false);
  echo is_array($http_response_header)."\n";

  /* NULL context */
  echo file_get_contents ($url, false, NULL);
  echo is_array($http_response_header)."\n";

  /* NULL context with offset and maxlen */
  echo file_get_contents ($url, false, NULL, 0, 50000);
  echo is_array($http_response_header)."\n";

  /* small maxlen */
  echo file_get_contents ($url, false, NULL, 0, 128);
  echo is_array($http_response_header)."\n";
}

f();
