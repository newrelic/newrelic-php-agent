<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test the use case where five parameters are passed into newrelic_notice_error and
ensure all parameters show up as error attributes.
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
                "error.class": "NoticedError",
                "error.message": "Test Error Has Occurred!",
                "transactionName": "??",
                "duration": "??",
                "nr.transactionGuid": "??",
                "guid": "??",
                "sampled": "??",
                "priority": "??",
                "traceId": "??",
                "spanId": "??"
            },
            {
                "user.error.message": "Test Error Has Occurred!",   
                "user.error.file": "Random.php",
                "user.error.line": 100,
                "user.error.context": "Random Error Has Been Detected",
                "user.error.number": 256
            },
            {}
        ]
    ]
]
*/

newrelic_notice_error(256, "Test Error Has Occurred!", "Random.php", 100, "Random Error Has Been Detected");
