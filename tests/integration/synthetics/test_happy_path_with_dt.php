<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should put the Synthetics metadata into the transaction trace
and the transaction event when a valid synthetics header is received.
Additionally, ensure a transaction trace is recorded regardless of the
trace threshold. With DT enabled, DT as well as Synthetics attributes have to
be added.
*/

/*
 * The synthetics header contains the following.
 *   [
 *     1,
 *     ENV[ACCOUNT_supportability],
 *     "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
 *     "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
 *     "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
 *   ]
 */

/*SKIPIF
<?php
if (!isset($_ENV["SYNTHETICS_HEADER_supportability"])) {
    die("skip: env vars required");
}
*/

/*INI
newrelic.distributed_tracing_enabled = 1
newrelic.cross_application_tracer.enabled = false
newrelic.transaction_tracer.threshold = '1h'
newrelic.transaction_tracer.detail = 0
newrelic.special.expensive_node_min = 0
*/

/*HEADERS
X-NewRelic-Synthetics=ENV[SYNTHETICS_HEADER_supportability]
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  {
    "reservoir_size": "??",
    "events_seen": 1
  },
  [
    [
      {
        "type": "Transaction",
        "name": "WebTransaction/Uri__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "nr.apdexPerfZone": "??",
        "nr.syntheticsResourceId": "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
        "nr.syntheticsJobId": "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
        "nr.syntheticsMonitorId": "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm",
        "externalDuration": "??",
	"externalCallCount": "1",
        "guid": "??",
        "sampled": true,
        "priority": "??",
        "traceId": "??",
        "error": false
      },
      "?? user attributes",
      "?? agent attributes"
    ]
  ]
]
*/

/*EXPECT_TXN_TRACES
[
  "?? agent run id",
  [
    [
      "?? entry",
      "?? duration",
      "WebTransaction/Uri__FILE__",
      "__FILE__",
      [
        [
          0, {}, {},
          "?? trace details",
          {
            "agentAttributes": "??",
            "intrinsics": {
              "totalTime": "??",
              "cpu_time": "??",
              "cpu_user_time": "??",
              "cpu_sys_time": "??",
              "guid": "??",
              "sampled": true,
              "priority": "??",
              "traceId": "??",
              "synthetics_resource_id": "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr",
              "synthetics_job_id": "jjjjjjj-jjjj-1234-jjjj-jjjjjjjjjjjj",
              "synthetics_monitor_id": "mmmmmmm-mmmm-1234-mmmm-mmmmmmmmmmmm"
            }
          }
        ],
        "?? string table"
      ],
      "?? txn guid",
      "?? reserved",
      "?? force persist",
      "?? x-ray sessions",
      "rrrrrrr-rrrr-1234-rrrr-rrrrrrrrrrrr"
    ]
  ]
]
*/

require_once(realpath(dirname(__FILE__)) . '/../../include/config.php');

$url = "http://" . make_tracing_url(realpath(dirname(__FILE__)) . '/../../include/tracing_endpoint.php');

function hello() {
  echo "hello, world!<br>\n";
}

/*
 * Call at least one user function to ensure the transaction trace is
 * not empty. Otherwise, it will be discarded by the agent.
 */
hello();

/*
 * This is necessary, as the transaction is only marked as outgoing DT
 * transaction once DT headers are created.
 */
file_get_contents($url);
