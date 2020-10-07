<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests newrelic_set_user_attributes() happy path.
*/

/*EXPECT_ANALYTICS_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "Transaction",
        "name": "OtherTransaction/php__FILE__",
        "timestamp": "??",
        "duration": "??",
        "totalTime": "??",
        "error": false
      },
      {
        "product": "my_product",
        "account": "my_account",
        "user": "my_user"
      },
      {
      }
    ]
  ]
]
*/

newrelic_set_user_attributes("my_user", "my_account", "my_product");
