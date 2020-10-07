<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

require_once(realpath (dirname ( __FILE__ )) . '/../../include/config.php');

function mongo_server()
{
  global $MONGO_HOST, $MONGO_PORT;

  return "mongodb://${MONGO_HOST}:${MONGO_PORT}";
}
