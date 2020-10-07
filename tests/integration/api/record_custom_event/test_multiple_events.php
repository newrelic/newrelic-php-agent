<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Tests multiple newrelic_record_custom_event() calls.
*/

/*INI
newrelic.custom_insights_events.enabled = 1
*/

/*EXPECT_CUSTOM_EVENTS
[
  "?? agent run id",
  "?? sampling information",
  [
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue0"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue1"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue2"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue3"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue4"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue5"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue6"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue7"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue8"
      },
      {
      }
    ],
    [
      {
        "type": "testType",
        "timestamp": "??"
      },
      {
        "testKey": "testValue9"
      },
      {
      }
    ]
  ]
]
*/

for ($i = 0; $i < 10; $i++) {
  newrelic_record_custom_event("testType", array("testKey"=>"testValue$i"));
}
