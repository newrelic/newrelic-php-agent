<?php
/*
 * Copyright 2022 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Test that Monolog3 instrumentation generates metrics and log events
*/

/*SKIPIF
<?php

require('skipif.inc');

*/

/*EXPECT
monolog3.DEBUG: debug []
monolog3.INFO: info []
monolog3.NOTICE: info []
monolog3.WARNING: warning []
monolog3.ERROR: error []
monolog3.CRITICAL: critical []
monolog3.ALERT: alert []
monolog3.EMERGENCY: emergency []
 */

require_once(realpath(dirname(__FILE__)) . '/../../../include/config.php');
require_once(realpath(dirname(__FILE__)) . '/../../../include/monolog.php');
require_monolog(3);

use Monolog\Logger;
use Monolog\Handler\StreamHandler;
use Monolog\Formatter\LineFormatter;


function test_logging() {
    $logger = new Logger('monolog3');

    $logfmt = "%channel%.%level_name%: %message% %context%\n";
    $formatter = new LineFormatter($logfmt);

    $stdoutHandler = new StreamHandler('php://stdout', Logger::DEBUG);
    $stdoutHandler->setFormatter($formatter);

    $logger->pushHandler($stdoutHandler);
    
    $logger->debug("debug");
    $logger->info("info");
    $logger->notice("info");
    $logger->warning("warning");
    $logger->error("error");
    $logger->critical("critical");
    $logger->alert("alert");
    $logger->emergency("emergency");
}

test_logging();