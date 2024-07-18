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
                "error.file": "random.php",
                "error.line": 46,
                "error.context": "Random Error Has Been Detected",
                "error.no": 256,
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

newrelic_notice_error(256, "Test Error Has Occurred!", "random.php", 46, "Random Error Has Been Detected");
