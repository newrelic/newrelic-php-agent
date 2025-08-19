<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests that remote_parent_not_sampled = 'always_off' works.
*/

/*INI
newrelic.transaction_events.attributes.include=request.uri
newrelic.distributed_tracing.sampler.remote_parent_sampled = 'always_off'
*/

/*HEADERS
X-Request-Start=1368811467146000
Content-Type=text/html
Accept=text/plain
User-Agent=Mozilla/5.0
Referer=http://user:pass@example.com/foo?q=bar#fragment
traceparent=00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-01
*/

/*ENVIRONMENT
REQUEST_METHOD=POST
CONTENT_LENGTH=348
*/

/*EXPECT
ok - insert function succeeded
ok - traceparent sampled flag ok
ok - tracestate sampled flag ok
ok - tracestate priority ok
*/

require_once(realpath (dirname ( __FILE__ )) . '/../../../include/tap.php');

header('Content-Type: text/html');
header('Content-Length: 41');

$outbound_headers = array('Accept-Language' => 'en-US,en;q=0.5');
tap_assert(newrelic_insert_distributed_trace_headers($outbound_headers), 'insert function succeeded');
$traceparent = explode('-', $outbound_headers['traceparent']);
$tracestate = explode('-', explode('=', $outbound_headers['tracestate'])[1]);

tap_equal($traceparent[3], '00', 'traceparent sampled flag ok');
tap_equal($tracestate[6], '0', 'tracestate sampled flag ok');
tap_not_equal($tracestate[7], '2.000000', 'tracestate priority ok');
