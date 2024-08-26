<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Ensure that all user attributes show up correctly when both 
newrelic_set_user_attributes and newrelic_notice_error are called.
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
                "product": "my_product",
                "account": "my_account",
                "user": "my_user",
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

newrelic_set_user_attributes("my_user", "my_account", "my_product");
newrelic_notice_error(256, "Test Error Has Occurred!", "Random.php", 100, "Random Error Has Been Detected");
