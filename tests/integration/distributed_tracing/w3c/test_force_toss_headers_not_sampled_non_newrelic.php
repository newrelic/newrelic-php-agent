<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
The agent should include web transaction attributes in error traces, error
events, analytic events and span events.
*/

/*INI
newrelic.transaction_events.attributes.include=request.uri
newrelic.distributed_tracing.sampler.remote_parent_not_sampled = 'always_off'
*/

/*HEADERS
X-Request-Start=1368811467146000
Content-Type=text/html
Accept=text/plain
User-Agent=Mozilla/5.0
Referer=http://user:pass@example.com/foo?q=bar#fragment
traceparent=00-87b1c9a429205b25e5b687d890d4821f-7d3efb1b173fecfa-00
*/

/*ENVIRONMENT
REQUEST_METHOD=POST
CONTENT_LENGTH=348
*/

/*EXPECT_SPAN_EVENTS
null
*/

header('Content-Type: text/html');
header('Content-Length: 41');
