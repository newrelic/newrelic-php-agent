<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test the agent's handling of malformed urls passed to curl_multi_exec().
 */

/*SKIPIF
<?php
if (!extension_loaded("curl")) {
  die("skip: curl extension required");
}
*/

/*EXPECT
ok - invalid url
ok - invalid url
ok - no more errors
*/

/*EXPECT_METRICS
[
  "?? agent run id",
  "?? start time",
  "?? stop time",
  [
    [{"name":"External/<unknown>/all"},                          [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/<unknown>/all",
      "scope":"OtherTransaction/php__FILE__"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"/External/(0\\.0\\.0\\.)?19/all/"},                [1, "??", "??", "??", "??", "??"]],
    [{"name":"/External/(0\\.0\\.0\\.)?19/all/",
      "scope":"OtherTransaction/php__FILE__"},                   [1, "??", "??", "??", "??", "??"]],
    [{"name":"External/all"},                                    [2, "??", "??", "??", "??", "??"]],
    [{"name":"External/allOther"},                               [2, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/all"},                            [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransaction/php__FILE__"},                    [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime"},                       [1, "??", "??", "??", "??", "??"]],
    [{"name":"OtherTransactionTotalTime/php__FILE__"},           [1, "??", "??", "??", "??", "??"]]
  ]
]
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

function test_curl() {
  $cm = curl_multi_init();

  $ch = curl_init();
  curl_setopt($ch, CURLOPT_NOBODY, true);
  curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch, CURLOPT_URL, '');
  curl_multi_add_handle($cm, $ch);

  $ch2 = curl_init();
  curl_setopt($ch2, CURLOPT_NOBODY, true);
  curl_setopt($ch2, CURLOPT_RETURNTRANSFER, true);
  curl_setopt($ch2, CURLOPT_URL, 19);
  curl_multi_add_handle($cm, $ch2);
  $active = 0;

  do {
    curl_multi_exec($cm, $active);
  } while ($active > 0);

  /* Non-0 result indicates an error */
  $info = curl_multi_info_read($cm);
  tap_ok('invalid url', $info["result"]);
  $info = curl_multi_info_read($cm);
  tap_ok('invalid url', $info["result"]);

  /* No more errors */
  tap_refute(curl_multi_info_read($cm), 'no more errors');

  curl_multi_remove_handle($cm, $ch);
  curl_multi_remove_handle($cm, $ch2);

  curl_close($ch);
  curl_close($ch2);
}

test_curl();
