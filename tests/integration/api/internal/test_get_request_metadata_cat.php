<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
newrelic_get_request_metadata() should return CAT headers for use in a request.
 */

/*INI
newrelic.distributed_tracing_enabled=0
newrelic.cross_application_tracer.enabled = true
newrelic.application_logging.forwarding.enabled = false
*/

/*EXPECT
ok - metadata is an array
ok - metadata has two elements
ok - X-NewRelic-ID is valid
ok - X-NewRelic-Transaction is valid
*/

require_once(realpath(dirname( __FILE__ )).'/../../../include/tap.php');

$metadata = newrelic_get_request_metadata();

tap_assert(is_array($metadata), 'metadata is an array');
tap_equal(2, count($metadata), 'metadata has two elements');
tap_equal(1, preg_match('#^[a-zA-Z0-9\=\+/]{20}$#', $metadata['X-NewRelic-ID']), 'X-NewRelic-ID is valid');
tap_equal(1, preg_match('#^[a-zA-Z0-9\=\+/]{76}$#', $metadata['X-NewRelic-Transaction']), 'X-NewRelic-Transaction is valid');
