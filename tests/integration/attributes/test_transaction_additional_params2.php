<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test the use case where two parameters are passed into newrelic_notice_error 
and the exception is shown as the error message.
*/

/*EXPECT_ERROR_EVENTS
[
    "?? agent run id",
    {
        "reservoir_size": 100,
        "events_seen": 1
    },
    [
        [
            {
                "type": "TransactionError",
                "timestamp": "??",
                "error.class": "Exception",
                "error.message": "Noticed exception 'Exception' with message 'Sample Exception' in __FILE__:??",
                "transactionName": "??",
                "duration": "??",
                "nr.transactionGuid": "??",
                "guid": "??",
                "sampled": "??",
                "priority": "??",
                "traceId": "??",
                "spanId": "??"
            },
            {},
            {}
        ]
    ]
]
*/

newrelic_notice_error("Test Error Has Occurred!", new Exception('Sample Exception'));
