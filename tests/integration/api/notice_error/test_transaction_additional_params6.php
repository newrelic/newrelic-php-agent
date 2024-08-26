<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Ensure null values are handled correctly when passed into newrelic_notice_error.
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
                "user.error.file": "Random.php",
                "user.error.context": "Random Error Has Been Detected",
                "user.error.line": 100,
                "user.error.number": 256
            },
            {}
        ]
    ]
]
*/

newrelic_notice_error(256, null, "Random.php", 100, "Random Error Has Been Detected");
